//
// framesync64_sim.c
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <complex.h>

#include "liquid.doc.h"

static int frame64_fer_callback(unsigned char *  _header,
                                int              _header_valid,
                                unsigned char *  _payload,
                                unsigned int     _payload_len,
                                int              _payload_valid,
                                framesyncstats_s _stats,
                                void *           _userdata);

typedef struct {
    int frame_detected;
    int header_decoded;
    int payload_decoded;
} frame64_fer_simdata;

// simulate frame detection and error rates for frame64
void frame64_fer(unsigned int  _num_frames,
                 float         _SNRdB,
                 int           _verbose,
                 fer_results * _results)
{
    // TODO: validate options
    // get options
    unsigned int num_frames = _num_frames;
    float noise_floor       = -40.0f;   // do not change
    float SNRdB             = _SNRdB;
    int verbose             = _verbose;

    // bookkeeping variables
    unsigned int i, j;
    frame64_fer_simdata simdata;

    // create framegen64 object
    framegen64 fg = framegen64_create();

    // frame synchronizer
    framesync64 fs = framesync64_create(frame64_fer_callback,(void*)&simdata);

    // allocate buffers
    unsigned char header[8];    // header data
    unsigned char payload[64];  // payload data
    float complex buffer[FRAME64_LEN]; // output frame buffer

    // channel objects
    nco_crcf nco_channel = nco_crcf_create(LIQUID_VCO);
    float nstd  = powf(10.0f, noise_floor/20.0f);         // noise std. dev.
    float gamma = powf(10.0f, (SNRdB+noise_floor)/20.0f); // channel gain

    //
    // ---------- BEGIN TRIALS ----------
    //

    // initalize results field
    _results->num_frames        = 0;
    _results->num_missed_frames = 0;
    _results->num_header_errors = 0;
    _results->num_packet_errors = 0;

    for (j=0; j<num_frames; j++) {
        // reset generator/synchronizer objects
        framesync64_reset(fs);

        _results->num_frames++;

        simdata.frame_detected  = 0;
        simdata.header_decoded  = 0;
        simdata.payload_decoded = 0;

        // generate random packet data, encode
        for (i=0; i<8; i++)
            header[i] = rand() & 0xff;
        for (i=0; i<64; i++)
            payload[i] = rand() & 0xff;

        // generate the frame
        framegen64_execute(fg,header,payload,buffer);

        // initialize frame synchronizer with noise
        for (i=0; i<(rand()%100)+400; i++) {
            float complex noise = nstd * (randnf() + _Complex_I*randnf()) * M_SQRT1_2;
            framesync64_execute(fs, &noise, 1);
        }

        // add channel impairments
        for (i=0; i<FRAME64_LEN; i++) {
            buffer[i] *= gamma;
            buffer[i] += nstd * (randnf() + _Complex_I*randnf()) * M_SQRT1_2;
        }

        // push frame through synchronizer
        framesync64_execute(fs, buffer, FRAME64_LEN);

        // flush frame synchronizer
        for (i=0; i<100; i++) {
            float complex noise = nstd * (randnf() + _Complex_I*randnf()) * M_SQRT1_2;
            framesync64_execute(fs, &noise, 1);
        }

        // accumulate results
        _results->num_missed_frames += simdata.frame_detected  ? 0 : 1;
        _results->num_header_errors += simdata.header_decoded  ? 0 : 1;
        _results->num_packet_errors += simdata.payload_decoded ? 0 : 1;

        // peridically print results
        if ( (((j+1) % 10)==0 || (j==num_frames-1)) && verbose) {
            float FER = (float) _results->num_missed_frames / (float) _results->num_frames;
            float HER = (float) _results->num_header_errors / (float) _results->num_frames;
            float PER = (float) _results->num_packet_errors / (float) _results->num_frames;

            printf(" SNR: %5.2f[%6u/%6u] frames:%6u(%5.1f%%) headers:%6u(%5.1f%%) packets:%6u(%5.1f%%)\r",
                    SNRdB,
                    j+1, num_frames,
                    _results->num_missed_frames, FER*100,
                    _results->num_header_errors, HER*100,
                    _results->num_packet_errors, PER*100);
            fflush(stdout);
        }

    } // num_frames
        
    // clean up objects
    framegen64_destroy(fg);
    framesync64_destroy(fs);
    nco_crcf_destroy(nco_channel);

    // update statistics
    _results->FER = (float) _results->num_missed_frames / (float) _results->num_frames;
    _results->HER = (float) _results->num_header_errors / (float) _results->num_frames;
    _results->PER = (float) _results->num_packet_errors / (float) _results->num_frames;

    if ( verbose) {
        // print new line (refesh buffer)
        printf("\n");
    }
    return;
}

// static callback function
static int frame64_fer_callback(unsigned char *  _header,
                                int              _header_valid,
                                unsigned char *  _payload,
                                unsigned int     _payload_len,
                                int              _payload_valid,
                                framesyncstats_s _stats,
                                void *           _userdata)
{
    frame64_fer_simdata * simdata = (frame64_fer_simdata*) _userdata;

    // specify that frame was detected
    simdata->frame_detected = 1;

    if (_header_valid)
        simdata->header_decoded = 1;

    if (_payload_valid)
        simdata->payload_decoded = 1;

    return 0;
}

