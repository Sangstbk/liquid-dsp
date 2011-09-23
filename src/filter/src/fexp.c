/*
 * Copyright (c) 2011 Joseph Gaeddert
 * Copyright (c) 2011 Virginia Polytechnic Institute & State University
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
// Flipped exponential filter
//
// References:
//   [Beaulieu:2001]
//   [Assalini:2004]
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "liquid.internal.h"

// Design fexp Nyquist filter
//  _k      : samples/symbol
//  _m      : symbol delay
//  _beta   : rolloff factor (0 < beta <= 1)
//  _dt     : fractional sample delay
//  _h      : output coefficient buffer (length: 2*k*m+1)
void liquid_firdes_fexp(unsigned int _k,
                        unsigned int _m,
                        float _beta,
                        float _dt,
                        float * _h)
{
    // validate input
    if ( _k < 1 ) {
        fprintf(stderr,"error: liquid_firdes_fexp(): k must be greater than 0\n");
        exit(1);
    } else if ( _m < 1 ) {
        fprintf(stderr,"error: liquid_firdes_fexp(): m must be greater than 0\n");
        exit(1);
    } else if ( (_beta < 0.0f) || (_beta > 1.0f) ) {
        fprintf(stderr,"error: liquid_firdes_fexp(): beta must be in [0,1]\n");
        exit(1);
    } else;

    unsigned int i;

    // derived values
    unsigned int h_len = 2*_k*_m+1;   // filter length

    float H_prime[h_len];   // frequency response of Nyquist filter (real)
    float complex H[h_len]; // frequency response of Nyquist filter
    float complex h[h_len]; // impulse response of Nyquist filter

    // compute Nyquist filter frequency response
    liquid_firdes_fexp_freqresponse(_k, _m, _beta, H_prime);

    // copy result to fft input buffer
    for (i=0; i<h_len; i++)
        H[i] = H_prime[i];

    // compute ifft
    fft_run(h_len, H, h, FFT_REVERSE, 0);
    
    // copy shifted, scaled response
    for (i=0; i<h_len; i++)
        _h[i] = crealf( h[(i+_k*_m+1)%h_len] ) * (float)_k / (float)(h_len);
}

// Design fexp square-root Nyquist filter
//  _k      : samples/symbol
//  _m      : symbol delay
//  _beta   : rolloff factor (0 < beta <= 1)
//  _dt     : fractional sample delay
//  _h      : output coefficient buffer (length: 2*k*m+1)
void liquid_firdes_rfexp(unsigned int _k,
                         unsigned int _m,
                         float _beta,
                         float _dt,
                         float * _h)
{
    // validate input
    if ( _k < 1 ) {
        fprintf(stderr,"error: liquid_firdes_rfexp(): k must be greater than 0\n");
        exit(1);
    } else if ( _m < 1 ) {
        fprintf(stderr,"error: liquid_firdes_rfexp(): m must be greater than 0\n");
        exit(1);
    } else if ( (_beta < 0.0f) || (_beta > 1.0f) ) {
        fprintf(stderr,"error: liquid_firdes_rfexp(): beta must be in [0,1]\n");
        exit(1);
    } else;
    
    unsigned int i;

    // derived values
    unsigned int h_len = 2*_k*_m+1;   // filter length

    float H_prime[h_len];   // frequency response of Nyquist filter (real)
    float complex H[h_len]; // frequency response of Nyquist filter
    float complex h[h_len]; // impulse response of Nyquist filter

    // compute Nyquist filter frequency response
    liquid_firdes_fexp_freqresponse(_k, _m, _beta, H_prime);

    // compute square root of result and copy to fft input buffer
    for (i=0; i<h_len; i++)
        H[i] = sqrtf( H_prime[i] );

    // compute ifft
    fft_run(h_len, H, h, FFT_REVERSE, 0);
    
    // copy shifted, scaled response
    for (i=0; i<h_len; i++)
        _h[i] = crealf( h[(i+_k*_m+1)%h_len] ) * (float)_k / (float)(h_len);
}

// flipped exponential frequency response
void liquid_firdes_fexp_freqresponse(unsigned int _k,
                                     unsigned int _m,
                                     float        _beta,
                                     float *      _H)
{
    // TODO : validate input

    unsigned int i;

    unsigned int h_len = 2*_k*_m + 1;

    float f0 = 0.5f*(1.0f - _beta) / (float)_k;
    float f1 = 0.5f*(1.0f        ) / (float)_k;
    float f2 = 0.5f*(1.0f + _beta) / (float)_k;

    float B     = 0.5f/(float)_k;
    float gamma = logf(2.0f)/(_beta*B);

    // compute frequency response of Nyquist filter
    for (i=0; i<h_len; i++) {
        float f = (float)i / (float)h_len;
        if (f > 0.5f) f = f - 1.0f;

        // enforce even symmetry
        f = fabsf(f);

        if ( f < f0 ) {
            // pass band
            _H[i] = 1.0f;
        } else if (f > f0 && f < f2) {
            // transition band
            if ( f < f1) {
                _H[i] = expf(gamma*(B*(1-_beta) - f));
            } else {
                _H[i] = 1.0f - expf(gamma*(f - (1+_beta)*B));
            }
        } else {
            // stop band
            _H[i] = 0.0f;
        }
    }
}
