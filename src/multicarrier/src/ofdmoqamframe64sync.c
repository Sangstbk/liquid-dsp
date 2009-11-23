/*
 * Copyright (c) 2007, 2009 Joseph Gaeddert
 * Copyright (c) 2007, 2009 Virginia Polytechnic Institute & State University
 *
 * This file is part of liquid.
 *
 * liquid is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * liquid is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with liquid.  If not, see <http://www.gnu.org/licenses/>.
 */

//
//
//

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "liquid.internal.h"

#define DEBUG_OFDMOQAMFRAME64SYNC             1
#define DEBUG_OFDMOQAMFRAME64SYNC_PRINT       1
#define DEBUG_OFDMOQAMFRAME64SYNC_FILENAME    "ofdmoqamframe64sync_internal_debug.m"
#define DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN  (2048)

// auto-correlation integration length
#define OFDMOQAMFRAME64SYNC_AUTOCORR_LEN      (64)

float ofdmoqamframe64sync_estimate_pilot_phase(float complex _y0,
                                               float complex _y1,
                                               float complex _p);

#if DEBUG_OFDMOQAMFRAME64SYNC
void ofdmoqamframe64sync_debug_print(ofdmoqamframe64sync _q);
#endif

struct ofdmoqamframe64sync_s {
    unsigned int num_subcarriers;   // 64
    unsigned int m;                 // filter delay
    float beta;                     // filter excess bandwidth factor

    // synchronizer parameters
    float rxx_thresh;   // auto-correlation threshold (0,1)
    float rxy_thresh;   // cross-correlation threshold (0,1)

    // filterbank objects
    firpfbch ca0;
    firpfbch ca1;
    float complex * X0, * X1;   // @ 64
    float complex * Y0, * Y1;   // @ 64

    // constants
    float zeta;         // scaling factor

    // PLCP
    float complex * S0; // short sequence
    float complex * S1; // long sequence
    float complex * S2; // training sequence
    float complex * S1a;    // decoded long sequence (first)
    float complex * S1b;    // decoded long sequence (second)

    // pilot sequence
    msequence ms_pilot;

    // signal detection | automatic gain control
    agc sigdet;

    // auto-correlators
    autocorr_cccf autocorr0;        // auto-correlation object [0]
    autocorr_cccf autocorr1;        // auto-correlation object [1]
    unsigned int autocorr_length;   // auto-correlation length
    unsigned int autocorr_delay0;   // delay [0]
    unsigned int autocorr_delay1;   // delay [1]
    float complex rxx0;
    float complex rxx1;
    float rxx_mag_max;

    // cross-correlator
    float complex * hxy;
    fir_filter_cccf crosscorr;

    // carrier frequency offset (CFO) estimation, compensation
    float nu_hat;
    nco nco_rx;     //numerically-controlled oscillator

    // gain
    float g;    // coarse gain estimate
    float complex * G0; // complex subcarrier gain estimate, S2[0]
    float complex * G1; // complex subcarrier gain estimate, S2[1]
    float complex * G;  // complex subcarrier gain estimate
    float phi_pilots[4];

    // receiver state
    enum {
        OFDMOQAMFRAME64SYNC_STATE_PLCPSHORT=0,  // seek PLCP short sequence
        OFDMOQAMFRAME64SYNC_STATE_PLCPLONG0,    // seek first PLCP long sequence
        OFDMOQAMFRAME64SYNC_STATE_PLCPLONG1,    // seek second PLCP long sequence
        OFDMOQAMFRAME64SYNC_STATE_RXSYMBOLS     // receive payload symbols
    } state;

    // timing/delay
    unsigned int timer;
    unsigned int num_symbols;
    unsigned int num_data_symbols;
    unsigned int num_samples;
    unsigned int sample_phase;
    cfwdelay delay0;
    cfwdelay delay1;

    // output data buffer, ready for demodulation
    float complex * data;

    ofdmoqamframe64sync_callback callback;
    void * userdata;

#if DEBUG_OFDMOQAMFRAME64SYNC
    cfwindow debug_x;
    cfwindow debug_rxx0;
    cfwindow debug_rxx1;
    cfwindow debug_rxy;
    cfwindow debug_framesyms;
#endif
};

ofdmoqamframe64sync ofdmoqamframe64sync_create(unsigned int _m,
                                               float _beta,
                                               ofdmoqamframe64sync_callback _callback,
                                               void * _userdata)
{
    ofdmoqamframe64sync q = (ofdmoqamframe64sync) malloc(sizeof(struct ofdmoqamframe64sync_s));
    q->num_subcarriers = 64;

    // validate input
    if (_m < 1) {
        fprintf(stderr,"error: ofdmoqamframe64sync_create(), filter delay must be > 0\n");
        exit(1);
    } else if (_beta < 0.0f) {
        fprintf(stderr,"error: ofdmoqamframe64sync_create(), filter excess bandwidth must be > 0\n");
        exit(1);
    }
    q->m = _m;
    q->beta = _beta;

    // synchronizer parameters
    q->rxx_thresh = 0.75f;  // auto-correlation threshold
    q->rxy_thresh = 0.75f;  // cross-correlation threshold

    q->zeta = 64.0f/sqrtf(52.0f);   // scaling factor
    
    // create analysis filter banks
    q->ca0 = firpfbch_create(q->num_subcarriers, q->m, q->beta, 0.0f /*dt*/,FIRPFBCH_ROOTNYQUIST,0/*gradient*/);
    q->ca1 = firpfbch_create(q->num_subcarriers, q->m, q->beta, 0.0f /*dt*/,FIRPFBCH_ROOTNYQUIST,0/*gradient*/);
    q->X0 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    q->X1 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    q->Y0 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    q->Y1 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
 
    // allocate memory for PLCP arrays
    q->S0 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    q->S1 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    q->S2 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    ofdmoqamframe64_init_S0(q->S0);
    ofdmoqamframe64_init_S1(q->S1);
    ofdmoqamframe64_init_S2(q->S2);
    unsigned int i;
    for (i=0; i<q->num_subcarriers; i++) {
        q->S0[i] *= q->zeta;
        q->S1[i] *= q->zeta;
        q->S2[i] *= q->zeta;
    }
    q->S1a = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    q->S1b = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));

    // set pilot sequence
    q->ms_pilot = msequence_create(8);

    // create agc | signal detection object
    q->sigdet = agc_create(1.0f, 0.001f);

    // create NCO for CFO compensation
    q->nco_rx = nco_create(LIQUID_VCO);

    // create auto-correlator objects
    q->autocorr_length = OFDMOQAMFRAME64SYNC_AUTOCORR_LEN;
    q->autocorr_delay0 = q->num_subcarriers / 4;
    q->autocorr_delay1 = q->num_subcarriers / 2;
    q->autocorr0 = autocorr_cccf_create(q->autocorr_length, q->autocorr_delay0);
    q->autocorr1 = autocorr_cccf_create(q->autocorr_length, q->autocorr_delay1);

    // create cross-correlator object
    q->hxy = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    ofdmoqam cs = ofdmoqam_create(q->num_subcarriers,q->m,q->beta,
                                  0.0f,   // dt
                                  OFDMOQAM_SYNTHESIZER,
                                  0);     // gradient
    for (i=0; i<2*(q->m); i++)
        ofdmoqam_execute(cs,q->S1,q->hxy);
    // time reverse, complex conjugate (same as fftshift for
    // this particular sequence)
    memmove(q->X0, q->hxy, 64*sizeof(float complex));
    for (i=0; i<64; i++)
        q->hxy[i] = conjf(q->X0[64-i-1]);
    // fftshift
    //fft_shift(q->hxy,64);
    q->crosscorr = fir_filter_cccf_create(q->hxy, q->num_subcarriers);
    ofdmoqam_destroy(cs);

    // delay buffers
    q->delay0 = cfwdelay_create((q->num_subcarriers));
    q->delay1 = cfwdelay_create((q->num_subcarriers)/2);

    // gain
    q->g = 1.0f;
    q->G0 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    q->G1 = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));
    q->G  = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));

    q->data = (float complex*) malloc((q->num_subcarriers)*sizeof(float complex));

    // reset object
    ofdmoqamframe64sync_reset(q);

#if DEBUG_OFDMOQAMFRAME64SYNC
    q->debug_x =        cfwindow_create(DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN);
    q->debug_rxx0=      cfwindow_create(DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN);
    q->debug_rxx1=      cfwindow_create(DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN);
    q->debug_rxy=       cfwindow_create(DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN);
    q->debug_framesyms= cfwindow_create(DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN);
#endif

    q->callback = _callback;
    q->userdata = _userdata;

    return q;
}

void ofdmoqamframe64sync_destroy(ofdmoqamframe64sync _q)
{
#if DEBUG_OFDMOQAMFRAME64SYNC
    ofdmoqamframe64sync_debug_print(_q);
    cfwindow_destroy(_q->debug_x);
    cfwindow_destroy(_q->debug_rxx0);
    cfwindow_destroy(_q->debug_rxx1);
    cfwindow_destroy(_q->debug_rxy);
    cfwindow_destroy(_q->debug_framesyms);
#endif

    // free analysis filter bank memory objects
    firpfbch_destroy(_q->ca0);
    firpfbch_destroy(_q->ca1);
    free(_q->X0);
    free(_q->X1);
    free(_q->Y0);
    free(_q->Y1);

    // clean up PLCP arrays
    free(_q->S0);
    free(_q->S1);
    free(_q->S2);
    free(_q->S1a);
    free(_q->S1b);

    // free pilot msequence object memory
    msequence_destroy(_q->ms_pilot);

    // free agc | signal detection object memory
    agc_destroy(_q->sigdet);

    // free NCO object memory
    nco_destroy(_q->nco_rx);

    // free auto-correlator memory objects
    autocorr_cccf_destroy(_q->autocorr0);
    autocorr_cccf_destroy(_q->autocorr1);

    // free cross-correlator memory objects
    fir_filter_cccf_destroy(_q->crosscorr);
    free(_q->hxy);

    // free gain arrays
    free(_q->G0);
    free(_q->G1);
    free(_q->G);

    // free data buffer
    free(_q->data);

    // free delay buffer
    cfwdelay_destroy(_q->delay0);
    cfwdelay_destroy(_q->delay1);

    // free main object memory
    free(_q);
}

void ofdmoqamframe64sync_print(ofdmoqamframe64sync _q)
{
    printf("ofdmoqamframe64sync:\n");
    printf("    num subcarriers     :   %-u\n", _q->num_subcarriers);
    printf("    m (filter delay)    :   %-u\n", _q->m);
    printf("    beta (excess b/w)   :   %8.6f\n", _q->beta);
}

void ofdmoqamframe64sync_reset(ofdmoqamframe64sync _q)
{
    // reset pilot sequence generator
    msequence_reset(_q->ms_pilot);

    // reset auto-correlators
    autocorr_cccf_clear(_q->autocorr0);
    autocorr_cccf_clear(_q->autocorr1);
    _q->rxx_mag_max = 0.0f;

    // reset frequency offset estimation, correction
    _q->nu_hat = 0.0f;
    nco_reset(_q->nco_rx);

    // clear delay buffer
    cfwdelay_clear(_q->delay0);
    cfwdelay_clear(_q->delay1);

    // clear analysis filter bank objects
    firpfbch_clear(_q->ca0);
    firpfbch_clear(_q->ca1);

    // reset gain parameters
    _q->g = 1.0f;   // coarse gain estimate
    unsigned int i;
    for (i=0; i<_q->num_subcarriers; i++)
        _q->G[i] = 1.0f;
    for (i=0; i<4; i++)
        _q->phi_pilots[i] = 0.0f;

    // reset state
    _q->state = OFDMOQAMFRAME64SYNC_STATE_PLCPSHORT;
    //_q->state = OFDMOQAMFRAME64SYNC_STATE_PLCPLONG0;
    _q->timer = 0;
    _q->num_symbols = 0;
    _q->num_data_symbols = 0;
    _q->num_samples = 0;
    _q->sample_phase = 0;
}

void ofdmoqamframe64sync_execute(ofdmoqamframe64sync _q,
                                 float complex * _x,
                                 unsigned int _n)
{
    unsigned int i;
    float complex x;
    for (i=0; i<_n; i++) {
        x = _x[i];
#if DEBUG_OFDMOQAMFRAME64SYNC
        cfwindow_push(_q->debug_x,x);
#endif
        _q->num_samples++;

        // coarse gain correction
        //x *= _q->g;
        
        // compensate for CFO
        //nco_mix_down(_q->nco_rx, x, &x);

        // push sample into analysis filter banks
        float complex x_delay0;
        float complex x_delay1;
        cfwdelay_read(_q->delay0,&x_delay0);
        cfwdelay_read(_q->delay1,&x_delay1);
        cfwdelay_push(_q->delay0,x);
        cfwdelay_push(_q->delay1,x);
        firpfbch_analyzer_push(_q->ca0, x_delay0);  // push input sample
        firpfbch_analyzer_push(_q->ca1, x_delay1);  // push delayed sample

        switch (_q->state) {
        case OFDMOQAMFRAME64SYNC_STATE_PLCPSHORT:
            ofdmoqamframe64sync_execute_plcpshort(_q,x);
            break;
        case OFDMOQAMFRAME64SYNC_STATE_PLCPLONG0:
            ofdmoqamframe64sync_execute_plcplong0(_q,x);
            break;
        case OFDMOQAMFRAME64SYNC_STATE_PLCPLONG1:
            ofdmoqamframe64sync_execute_plcplong1(_q,x);
            break;
        case OFDMOQAMFRAME64SYNC_STATE_RXSYMBOLS:
            ofdmoqamframe64sync_execute_rxsymbols(_q,x);
            break;
        default:;
        }

    } // for (i=0; i<_n; i++)
} // ofdmoqamframe64sync_execute()

//
// internal
//

#if DEBUG_OFDMOQAMFRAME64SYNC
void ofdmoqamframe64sync_debug_print(ofdmoqamframe64sync _q)
{
    FILE * fid = fopen(DEBUG_OFDMOQAMFRAME64SYNC_FILENAME,"w");
    if (!fid) {
        printf("error: ofdmoqamframe64_debug_print(), could not open file for writing\n");
        return;
    }
    fprintf(fid,"%% %s : auto-generated file\n", DEBUG_OFDMOQAMFRAME64SYNC_FILENAME);
    fprintf(fid,"close all;\n");
    fprintf(fid,"clear all;\n");
    fprintf(fid,"n = %u;\n", DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN);
    unsigned int i;
    float complex * rc;

    // gain vectors
    fprintf(fid,"g = %12.4e;\n", _q->g);
    for (i=0; i<_q->num_subcarriers; i++) {
        fprintf(fid,"G(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(_q->G[i]), cimagf(_q->G[i]));
        //fprintf(fid,"G0(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(_q->G0[i]), cimagf(_q->G0[i]));
        //fprintf(fid,"G1(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(_q->G1[i]), cimagf(_q->G1[i]));
    }
    for (i=0; i<4; i++) {
        fprintf(fid,"phi(%3u) = %12.8f;\n", i+1, _q->phi_pilots[i]);
    }
    fprintf(fid,"figure;\n");
    fprintf(fid,"f = -32:31;\n");
    fprintf(fid,"subplot(2,1,1);\n");
    fprintf(fid,"    plot(f,fftshift(abs(G)));\n");
    fprintf(fid,"    xlabel('subcarrier index');\n");
    fprintf(fid,"    ylabel('|G|');\n");
    fprintf(fid,"    grid on;\n");
    fprintf(fid,"subplot(2,1,2);\n");
    fprintf(fid,"    plot(f,fftshift(arg(G)));\n");
    fprintf(fid,"    xlabel('subcarrier index');\n");
    fprintf(fid,"    ylabel('arg\\{G\\}');\n");
    fprintf(fid,"    grid on;\n");

    // CFO estimate
    fprintf(fid,"nu_hat = %12.4e;\n", _q->nu_hat);

    // short, long, training sequences
    for (i=0; i<_q->num_subcarriers; i++) {
        fprintf(fid,"S0(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(_q->S0[i]), cimagf(_q->S0[i]));
        fprintf(fid,"S1(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(_q->S1[i]), cimagf(_q->S1[i]));
        fprintf(fid,"S2(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(_q->S2[i]), cimagf(_q->S2[i]));
    }

    fprintf(fid,"x = zeros(1,n);\n");
    cfwindow_read(_q->debug_x, &rc);
    for (i=0; i<DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN; i++)
        fprintf(fid,"x(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(rc[i]), cimagf(rc[i]));
    fprintf(fid,"figure;\n");
    fprintf(fid,"plot(0:(n-1),real(x),0:(n-1),imag(x));\n");
    fprintf(fid,"xlabel('sample index');\n");
    fprintf(fid,"ylabel('received signal, x');\n");

    fprintf(fid,"rxx0 = zeros(1,n);\n");
    cfwindow_read(_q->debug_rxx0, &rc);
    for (i=0; i<DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN; i++)
        fprintf(fid,"rxx0(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(rc[i]), cimagf(rc[i]));
    fprintf(fid,"rxx1 = zeros(1,n);\n");
    cfwindow_read(_q->debug_rxx1, &rc);
    for (i=0; i<DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN; i++)
        fprintf(fid,"rxx1(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(rc[i]), cimagf(rc[i]));
    fprintf(fid,"figure;\n");
    fprintf(fid,"plot(0:(n-1),abs(rxx0),0:(n-1),abs(rxx1),0:(n-1),[abs(rxx0)+abs(rxx1)]/2,'-k','LineWidth',2);\n");
    fprintf(fid,"xlabel('sample index');\n");
    fprintf(fid,"ylabel('|r_{xx}|');\n");

    fprintf(fid,"rxy = zeros(1,n);\n");
    cfwindow_read(_q->debug_rxy, &rc);
    for (i=0; i<DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN; i++)
        fprintf(fid,"rxy(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(rc[i]), cimagf(rc[i]));
    fprintf(fid,"figure;\n");
    fprintf(fid,"plot(0:(n-1),abs(rxy));\n");
    fprintf(fid,"xlabel('sample index');\n");
    fprintf(fid,"ylabel('|r_{xy}|');\n");
    // decoded long sequences
    fprintf(fid,"S1  = zeros(1,64);\n");
    fprintf(fid,"S1a = zeros(1,64);\n");
    fprintf(fid,"S1b = zeros(1,64);\n");
    fprintf(fid,"Y0  = zeros(1,64);\n");
    fprintf(fid,"Y1  = zeros(1,64);\n");
    for (i=0; i<64; i++) {
        fprintf(fid,"S1(%4u)  = %12.4e + j*%12.4e;\n", i+1, crealf(_q->S1[i]),  cimagf(_q->S1[i]));
        fprintf(fid,"S1a(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(_q->S1a[i]), cimagf(_q->S1a[i]));
        fprintf(fid,"S1b(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(_q->S1b[i]), cimagf(_q->S1b[i]));
        fprintf(fid,"Y0(%4u)  = %12.4e + j*%12.4e;\n", i+1, crealf(_q->Y0[i]),  cimagf(_q->Y0[i]));
        fprintf(fid,"Y1(%4u)  = %12.4e + j*%12.4e;\n", i+1, crealf(_q->Y1[i]),  cimagf(_q->Y1[i]));
    }
    fprintf(fid,"zeta = 64/sqrt(52/2);\n");
    fprintf(fid,"S1  = S1 / zeta;\n");
    fprintf(fid,"S1a = S1a / zeta;\n");
    fprintf(fid,"S1b = S1b / zeta;\n");
    fprintf(fid,"Y0  = Y0  / zeta;\n");
    fprintf(fid,"Y1  = Y1  / zeta;\n");
    fprintf(fid,"t0 = 1:2:64;\n");
    fprintf(fid,"t1 = 2:2:64;\n");
    fprintf(fid,"S = zeros(1,64);\n");
    fprintf(fid,"S(t0) = real(S1a(t0)) + j*imag(S1b(t0));\n");
    fprintf(fid,"S(t1) = real(S1b(t1)) + j*imag(S1a(t1));\n");
    fprintf(fid,"Y = zeros(1,64);\n");
    fprintf(fid,"Y(t0) = real(Y0(t0)) + j*imag(Y1(t0));\n");
    fprintf(fid,"Y(t1) = real(Y1(t1)) + j*imag(Y0(t1));\n");

    fprintf(fid,"figure;\n");
    //fprintf(fid,"f = [(0:63)]/64 - 0.5;\n");
    //fprintf(fid,"plot(S1,'x',S1a,'x',S1b,'x');\n");
    fprintf(fid,"plot(S1,'x',S,'x',S1a,'x',S1b,'x');\n");
    fprintf(fid,"legend('S1','S','S1a','S1b',0);\n");
    fprintf(fid,"xlabel('I');\n");
    fprintf(fid,"ylabel('Q');\n");
    fprintf(fid,"axis square;\n");
    fprintf(fid,"axis([-1 1 -1 1]*1.3);\n");
    fprintf(fid,"title('PLCP long sequences');\n");

    // frame symbols
    fprintf(fid,"framesyms = zeros(1,n);\n");
    cfwindow_read(_q->debug_framesyms, &rc);
    for (i=0; i<DEBUG_OFDMOQAMFRAME64SYNC_BUFFER_LEN; i++)
        fprintf(fid,"framesyms(%4u) = %12.4e + j*%12.4e;\n", i+1, crealf(rc[i]), cimagf(rc[i]));
    fprintf(fid,"figure;\n");
    fprintf(fid,"plot(real(framesyms),imag(framesyms),'x','MarkerSize',1);\n");
    fprintf(fid,"axis square;\n");
    fprintf(fid,"axis([-1.5 1.5 -1.5 1.5]);\n");
    fprintf(fid,"xlabel('in-phase');\n");
    fprintf(fid,"ylabel('quadrature phase');\n");
    fprintf(fid,"title('Frame Symbols');\n");

    fclose(fid);
    printf("ofdmoqamframe64sync/debug: results written to %s\n", DEBUG_OFDMOQAMFRAME64SYNC_FILENAME);
}
#endif

// run analyzers and push resulting symbols to buffer, but
// do not increment symbol counter (num_symbols)
void ofdmoqamframe64sync_run_analyzers(ofdmoqamframe64sync _q)
{
    // run analyzers
    firpfbch_analyzer_run(_q->ca0, _q->X0);
    firpfbch_analyzer_run(_q->ca1, _q->X1);
}


void ofdmoqamframe64sync_execute_plcpshort(ofdmoqamframe64sync _q, float complex _x)
{
    // run AGC, clip output
    float complex y;
    agc_execute(_q->sigdet, _x, &y);
    //if (agc_get_signal_level(_q->sigdet) < -15.0f)
    //    return;

    // auto-correlators
    autocorr_cccf_push(_q->autocorr0, _x);
    autocorr_cccf_execute(_q->autocorr0, &_q->rxx0);

    autocorr_cccf_push(_q->autocorr1, _x);
    autocorr_cccf_execute(_q->autocorr1, &_q->rxx1);

    // TODO : compensate for signal level appropriately
    float g = agc_get_gain(_q->sigdet);
    g = 1.0f;
    _q->rxx0 *= g;
    _q->rxx1 *= g;

#if DEBUG_OFDMOQAMFRAME64SYNC
    cfwindow_push(_q->debug_rxx0, _q->rxx0);
    cfwindow_push(_q->debug_rxx1, _q->rxx1);
#endif
    float rxx_mag0 = cabsf(_q->rxx0);
    float rxx_mag1 = cabsf(_q->rxx1);

    float threshold = (_q->rxx_thresh)*(_q->autocorr_length);

    if (rxx_mag0 > threshold && rxx_mag1 > threshold) {
        // wait for auto-correlation to peak before changing state
        if (rxx_mag0 + rxx_mag1 > _q->rxx_mag_max) {
            _q->rxx_mag_max = rxx_mag0 + rxx_mag1;
            return;
        }

        // estimate CFO
        _q->nu_hat = cargf(_q->rxx0);
        if (_q->nu_hat >  M_PI/2.0f) _q->nu_hat -= M_PI;
        if (_q->nu_hat < -M_PI/2.0f) _q->nu_hat += M_PI;
        _q->nu_hat *= 4.0f / (float)(_q->num_subcarriers);

#if DEBUG_OFDMOQAMFRAME64SYNC_PRINT
        printf("rxx[0] = |%12.8f| arg{%12.8f}\n", cabsf(_q->rxx0),cargf(_q->rxx0));
        printf("rxx[1] = |%12.8f| arg{%12.8f}\n", cabsf(_q->rxx1),cargf(_q->rxx1));
        printf("nu_hat =  %12.8f\n", _q->nu_hat);
#endif

        nco_set_frequency(_q->nco_rx, _q->nu_hat);
        _q->state = OFDMOQAMFRAME64SYNC_STATE_PLCPLONG0;

        _q->g = agc_get_gain(_q->sigdet);
        _q->timer=0;
    }
}

void ofdmoqamframe64sync_execute_plcplong0(ofdmoqamframe64sync _q, float complex _x)
{
    // cross-correlator
    float complex rxy;
    fir_filter_cccf_push(_q->crosscorr, _x);
    fir_filter_cccf_execute(_q->crosscorr, &rxy);

#if DEBUG_OFDMOQAMFRAME64SYNC
    cfwindow_push(_q->debug_rxy, rxy);
#endif

    _q->timer++;
    if (_q->timer > 10*(_q->num_subcarriers)) {
        printf("warning: ofdmoqamframe64sync could not find first PLCP long sequence; resetting synchronizer\n");
        ofdmoqamframe64sync_reset(_q);
        return;
    }

    if (cabsf(rxy) > (_q->rxy_thresh)*(_q->num_subcarriers)) {
        printf("rxy[0] : %12.8f at input[%3u]\n", cabsf(rxy), _q->num_samples);

        // run analyzers
        //firpfbch_analyzer_run(_q->ca0, _q->S1a);
        //firpfbch_analyzer_run(_q->ca1, _q->S1b);
        ofdmoqamframe64sync_run_analyzers(_q);

        _q->sample_phase = (_q->num_samples + (_q->num_subcarriers)/2) % _q->num_subcarriers;
        //_q->sample_phase = (_q->num_samples) % _q->num_subcarriers;
        printf("sample phase : %u\n",_q->sample_phase);

        _q->state = OFDMOQAMFRAME64SYNC_STATE_PLCPLONG1;
        _q->timer = 0;
    }
}

void ofdmoqamframe64sync_execute_plcplong1(ofdmoqamframe64sync _q, float complex _x)
{
    // cross-correlator
    float complex rxy;
    fir_filter_cccf_push(_q->crosscorr, _x);
    fir_filter_cccf_execute(_q->crosscorr, &rxy);

#if DEBUG_OFDMOQAMFRAME64SYNC
    cfwindow_push(_q->debug_rxy, rxy);
#endif

    _q->timer++;
    if (_q->timer < _q->num_subcarriers-2) {
        return;
    } else if (_q->timer > _q->num_subcarriers+2) {
        printf("warning: ofdmoqamframe64sync could not find second PLCP long sequence; resetting synchronizer\n");
        ofdmoqamframe64sync_reset(_q);
        return;
    }

    if (cabsf(rxy) > (_q->rxy_thresh)*(_q->num_subcarriers)) {
        printf("rxy[1] : %12.8f at input[%3u]\n", cabsf(rxy), _q->num_samples);

        _q->state = OFDMOQAMFRAME64SYNC_STATE_RXSYMBOLS;
    }

}

void ofdmoqamframe64sync_execute_rxsymbols(ofdmoqamframe64sync _q, float complex _x)
{
    unsigned int k=_q->sample_phase;
    if ((_q->num_samples % 64) == k) {
        // run analyzers
        firpfbch_analyzer_run(_q->ca0, _q->Y0);
        firpfbch_analyzer_run(_q->ca1, _q->Y1);
        firpfbch_analyzer_saverunstate(_q->ca0);
        firpfbch_analyzer_saverunstate(_q->ca1);
        
        printf("symbol[%3u] : %4u\n", _q->num_symbols, _q->num_samples);

        // determine what type of symbol is produced at the
        // output of the analysis filter banks, compensating
        // for the filter delay (e.g. short/long sequence,
        // data symbol. etc.)

        if (_q->num_symbols == _q->m) {
            // save first S1 symbol
        } else if (_q->num_symbols == _q->m + 1) {
            // save second S1 symbol
        } else if (_q->num_symbols == _q->m + 2) {
            // ignore first S2 symbol, S2[0]
        } else if (_q->num_symbols == _q->m + 3) {
            // save S2[1]
            printf("  estimating gain...\n");
            ofdmoqamframe64sync_estimate_gain_plcplong(_q);
            memmove(_q->S1a, _q->Y0, (_q->num_subcarriers)*sizeof(float complex));
            memmove(_q->S1b, _q->Y1, (_q->num_subcarriers)*sizeof(float complex));
            /*
            printf("exiting prematurely\n");
            ofdmoqamframe64sync_destroy(_q);
            exit(1);
            */
        } else if (_q->num_symbols == _q->m + 4) {
            // save S2[2], estimate gain
        } else if (_q->num_symbols > _q->m + 4) {
            // execute rxpayload
            ofdmoqamframe64sync_rxpayload(_q,_q->Y0,_q->Y1);
            //printf("  ==> data symbol %u\n", _q->num_data_symbols);
            _q->num_data_symbols++;
        }

        _q->num_symbols++;
    }
}

void ofdmoqamframe64sync_rxpayload(ofdmoqamframe64sync _q,
                                   float complex * _Y0,
                                   float complex * _Y1)
{
    unsigned int pilot_phase = msequence_advance(_q->ms_pilot);

    // gain correction (equalizer)
    unsigned int i;
    for (i=0; i<_q->num_subcarriers; i++) {
        _Y0[i] *= _q->G[i];// * _q->zeta;
        _Y1[i] *= _q->G[i];// * _q->zeta;
    }

    // TODO : extract pilots

    // TODO : compute pilot phase correction (fit to first-order polynomial)

    // strip data subcarriers
    unsigned int j=0;
    unsigned int t=0;
    int sctype;
    for (i=0; i<_q->num_subcarriers; i++) {
        sctype = ofdmoqamframe64_getsctype(i);
        if (sctype==OFDMOQAMFRAME64_SCTYPE_NULL) {
            // disabled subcarrier
        } else if (sctype==OFDMOQAMFRAME64_SCTYPE_PILOT) {
            // pilot subcarrier : use p/n sequence for pilot phase
            float complex y0 = ((i%2)==0 ? _Y0[i] : _Y1[i]) / _q->zeta;
            float complex y1 = ((i%2)==0 ? _Y1[i] : _Y0[i]) / _q->zeta;
            float complex p = (pilot_phase ? 1.0f : -1.0f);
            printf("i = %u\n", i);
            printf("y0 = %12.8f + j*%12.8f;\n", crealf(y0),cimagf(y0));
            printf("y1 = %12.8f + j*%12.8f;\n", crealf(y1),cimagf(y1));
            float phi_hat =
            ofdmoqamframe64sync_estimate_pilot_phase(y0,y1,p);
            _q->phi_pilots[t++] = phi_hat;
            /*
            printf("exiting prematurely\n");
            ofdmoqamframe64sync_destroy(_q);
            exit(1);
            */
        } else {
            // data subcarrier
            if ((i%2)==0) {
                // even subcarrier
                _q->data[j] = crealf(_Y0[i]) + 
                              cimagf(_Y1[i]) * _Complex_I;
            } else {
                // odd subcarrier
                _q->data[j] = cimagf(_Y0[i]) * _Complex_I +
                              crealf(_Y1[i]);
            }
            // scaling factor
            _q->data[j] /= _q->zeta;
            j++;
        }
    }
    assert(j==48);

//#if DEBUG_OFDMFRAME64SYNC
    for (i=0; i<48; i++)
        cfwindow_push(_q->debug_framesyms,_q->data[i]);
//#endif

    if (_q->callback != NULL) {
        int retval = _q->callback(_q->data, _q->userdata);
        if (retval == -1) {
            printf("exiting prematurely\n");
            ofdmoqamframe64sync_destroy(_q);
            exit(0);
        } else if (retval == 1) {
#if DEBUG_OFDMOQAMFRAME64SYNC_PRINT
            printf("resetting synchronizer\n");
#endif
            ofdmoqamframe64sync_reset(_q);
        } else {
            // do nothing
        }
    }

}

void ofdmoqamframe64sync_estimate_gain_plcplong(ofdmoqamframe64sync _q)
{
    unsigned int j;
    int sctype;
    for (j=0; j<_q->num_subcarriers; j++) {
        sctype = ofdmoqamframe64_getsctype(j);
        if (sctype==OFDMOQAMFRAME64_SCTYPE_NULL) {
            // disabled subcarrier
            _q->G[j] = 0.0f;
        } else {
            if ((j%2)==0) {
                // even
                _q->G[j] = 1.0f;
            } else {
                // odd
                _q->G[j] = 1.0f / (_q->Y0[j] / _q->S2[j]);
            }
        }
    }

    // run the loop again, computing gains on even subcarriers
    for (j=0; j<_q->num_subcarriers; j++) {
        sctype = ofdmoqamframe64_getsctype(j);
        if (sctype==OFDMOQAMFRAME64_SCTYPE_NULL) {
            continue;
        } else if ((j%2)==1) {
            continue;
        } else {
            // even subcarrier: average adjacent subcarriers' gains
            // TODO : improve upon this method
            unsigned int j0 = (j+_q->num_subcarriers-1)%_q->num_subcarriers;
            unsigned int j1 = (j+1)%_q->num_subcarriers;
            int sctype0 = ofdmoqamframe64_getsctype(j0);
            int sctype1 = ofdmoqamframe64_getsctype(j1);
            float complex g=0.0f;
            float w=0.0f;
            if (sctype0 != OFDMOQAMFRAME64_SCTYPE_NULL) {
                g += _q->G[j0];
                w += 1.0f;
            }
            if (sctype1 != OFDMOQAMFRAME64_SCTYPE_NULL) {
                g += _q->G[j1];
                w += 1.0f;
            }
            _q->G[j] = g / w;
        }
    }
    // TODO : run smoothing algorithm
}

float ofdmoqamframe64sync_estimate_pilot_phase(float complex _y0,
                                               float complex _y1,
                                               float complex _p)
{
    float e_min = 0.0f;
    float complex e = 0.0f;
    float phi_hat = 0.0f;
    float phi0 = -M_PI/1.0f;
    float phi1 =  M_PI/1.0f;

    float phi  = phi0;
    float dphi = M_PI/100.0f;
    float complex g;
    float complex y_hat, y0_hat, y1_hat;
    unsigned int i=0;

    FILE * fid = fopen("ofdmoqamframe64sync_estimate_pilot_phase.m","w");
    fprintf(fid,"close all;\n");
    fprintf(fid,"clear all;\n");
    while (phi <= phi1) {
        // compute complex phase rotation gain
        g = cexpf(-_Complex_I*phi);

        // compute rotation
        y0_hat = _y0*g;
        y1_hat = _y1*g;
        y_hat = crealf(y0_hat) + _Complex_I*cimagf(y1_hat);
        //y_hat = crealf(y1_hat) + _Complex_I*cimagf(y0_hat);

        // compute error
        e = fabsf(crealf(y_hat)-crealf(_p)) +
            fabsf(cimagf(y_hat)-crealf(_p));
        e = cabsf(y_hat-_p);
        e = y_hat - _p;

        if (cabsf(e) < e_min || i==0) {
            e_min = e;
            phi_hat = phi;
        }

        //printf("e(%3u) = %12.8f; phi(%3u) = %12.8f; pause(0.02);\n", i+1, e, i+1, phi);
        fprintf(fid,"y_hat(%3u) = %12.8f + j*%12.8f;\n", i+1, crealf(y_hat), cimagf(y_hat));
        fprintf(fid,"e(%3u) = %12.8f + j*%12.8f; phi(%3u) = %12.8f;\n", i+1, crealf(e),cimagf(e), i+1, phi);
        i++;

        // increment phase
        phi += dphi;
    }
    fprintf(fid,"p0 = %12.8f + j*%12.8f;\n", crealf(_y0), cimagf(_y0));
    fprintf(fid,"p1 = %12.8f + j*%12.8f;\n", crealf(_y1), cimagf(_y1));
    fprintf(fid,"p = %12.8f + j*%12.8f;\n", crealf(_p), cimagf(_p));
    //printf("phi_hat : %12.8f\n", phi_hat);
    fprintf(fid,"figure;\n");
    fprintf(fid,"plot(real(y_hat),imag(y_hat),'-',real(y_hat(1)),imag(y_hat(1)),'rx',real(p),imag(p),'os');\n");
    fprintf(fid,"axis([-1 1 -1 1]*1.5);\n");
    fprintf(fid,"axis('square');\n");
    fprintf(fid,"grid on;\n");
    fprintf(fid,"figure;\n");
    fprintf(fid,"plot(phi,real(e),phi,imag(e),phi,abs(e)); grid on;\n");
    fclose(fid);
    return phi_hat;
}