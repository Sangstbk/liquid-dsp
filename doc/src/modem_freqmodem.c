// 
// modem_freqmodem.c
//
// Tests simple FM modulation/demodulation
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "liquid.h"
#include "liquid.doc.h"

#define OUTPUT_FILENAME_TIME "figures.gen/modem_freqmodem_time.gnu"
#define OUTPUT_FILENAME_FREQ "figures.gen/modem_freqmodem_freq.gnu"

int main(int argc, char*argv[])
{
    // options
    float kf = 0.05f;               // modulation factor
    liquid_freqdem_type type = LIQUID_FREQDEM_DELAYCONJ;
    unsigned int num_samples = 201; // number of samples
    float SNRdB = 60.0f;            // signal-to-noise ratio [dB]

    // derived values
    unsigned int nfft = 1024;

    // create mod/demod objects
    freqmod mod   = freqmod_create(kf);
    freqdem demod = freqdem_create(kf,type);
    freqmod_print(mod);

    unsigned int i;
    float x[num_samples];
    float complex y[num_samples];
    float z[num_samples];

#if 0
    // generate input data: windowed sinusoid
    float f_audio = 0.04179f;   // input sine frequency
    for (i=0; i<num_samples; i++) {
        x[i] = sinf(2*M_PI*i*f_audio) + 0.5f*sinf(2*M_PI*i*f_audio*1.8f);
        x[i] *= 0.8f*hamming(i,num_samples);
    }
#else
    // generate un-modulated input signal (filtered noise)
    float fc_audio = 0.07f;
    float f0_audio = 0.10f;
    iirfilt_rrrf f = iirfilt_rrrf_create_prototype(LIQUID_IIRDES_BUTTER,
                                                   LIQUID_IIRDES_BANDPASS,
                                                   LIQUID_IIRDES_SOS,
                                                   6, fc_audio, f0_audio, 1.0f, 1.0f);

    // push noise through filter
    for (i=0; i<100; i++)
        iirfilt_rrrf_execute(f, 1.5f*randnf(), &x[0]);
    // generate filtered/windowed output
    for (i=0; i<num_samples; i++) {
        iirfilt_rrrf_execute(f, 1.5f*randnf(), &x[i]);

        x[i] *= hamming(i,num_samples);
    }
    iirfilt_rrrf_destroy(f);
#endif

    // modulate signal
    for (i=0; i<num_samples; i++)
        freqmod_modulate(mod, x[i], &y[i]);

    // add noise
    float nstd = powf(10.0f,-SNRdB/20.0f);
    for (i=0; i<num_samples; i++)
        cawgn(&y[i], nstd);

    // demodulate signal
    for (i=0; i<num_samples; i++)
        freqdem_demodulate(demod, y[i], &z[i]);
    
    // destroy objects
    freqmod_destroy(mod);
    freqdem_destroy(demod);

    // compute power spectral density
    float complex Y[nfft];
    liquid_doc_psdwindow wtype = LIQUID_DOC_PSDWINDOW_HANN;
    int normalize = 1;
    liquid_doc_compute_psdcf(y, num_samples, Y, nfft, wtype, normalize);

    // 
    // export output files
    //
    FILE * fid;

    // time-domain plot
    fid = fopen(OUTPUT_FILENAME_TIME,"w");
    if (!fid) {
        fprintf(stderr,"error: %s, could not open '%s' for writing\n", argv[0], OUTPUT_FILENAME_TIME);
        exit(1);
    }
    fprintf(fid,"# %s: auto-generated file\n\n", OUTPUT_FILENAME_TIME);
    fprintf(fid,"reset\n");
    fprintf(fid,"set terminal postscript eps enhanced color solid rounded\n");
    fprintf(fid,"set xrange [0:%u];\n",num_samples-1);
    fprintf(fid,"set yrange [-1.5:1.5]\n");
    fprintf(fid,"set size ratio 0.3\n");
    fprintf(fid,"set xlabel 'Sample Index'\n");
    fprintf(fid,"set ytics -5,0.5,5\n");
    fprintf(fid,"set grid xtics ytics\n");
    fprintf(fid,"set pointsize 0.6\n");
    fprintf(fid,"set grid linetype 1 linecolor rgb '%s' lw 1\n", LIQUID_DOC_COLOR_GRID);
    fprintf(fid,"set multiplot layout 2,1 scale 1.0,1.0\n");

    fprintf(fid,"# input/demodulated signals\n");
    fprintf(fid,"set ylabel 'input/output signal'\n");
    fprintf(fid,"set key top right nobox\n");
    fprintf(fid,"plot '-' using 1:2 with lines linetype 1 linewidth 2 linecolor rgb '%s' title 'input',\\\n",LIQUID_DOC_COLOR_RED);
    fprintf(fid,"     '-' using 1:2 with lines linetype 1 linecolor rgb '%s' title 'demodulated'\n",LIQUID_DOC_COLOR_GRAY);
    for (i=0; i<num_samples; i++)
        fprintf(fid,"%12u %12.4e\n", i, x[i]);
    fprintf(fid,"e\n");
    for (i=0; i<num_samples; i++)
        fprintf(fid,"%12d %12.4e\n", ((int)i)-8, z[i]);
    fprintf(fid,"e\n");

    fprintf(fid,"# transmitted signal\n");
    fprintf(fid,"set key bottom right nobox\n");
    fprintf(fid,"set ylabel 'modulated signal'\n");
    fprintf(fid,"plot '-' using 1:2 with lines linetype 1 linewidth 1 linecolor rgb '%s' title 'real',\\\n",LIQUID_DOC_COLOR_BLUE);
    fprintf(fid,"     '-' using 1:2 with lines linetype 1 linewidth 1 linecolor rgb '%s' title 'imag'\n",LIQUID_DOC_COLOR_GREEN);
    for (i=0; i<num_samples; i++)
        fprintf(fid,"%12u %12.4e\n", i, crealf(y[i]));
    fprintf(fid,"e\n");
    for (i=0; i<num_samples; i++)
        fprintf(fid,"%12u %12.4e\n", i, cimagf(y[i]));
    fprintf(fid,"e\n");

    fclose(fid);
    printf("results written to '%s\n", OUTPUT_FILENAME_TIME);


    // frequency-domain plot
    fid = fopen(OUTPUT_FILENAME_FREQ,"w");
    if (!fid) {
        fprintf(stderr,"error: %s, could not open '%s' for writing\n", argv[0], OUTPUT_FILENAME_FREQ);
        exit(1);
    }
    fprintf(fid,"# %s: auto-generated file\n\n", OUTPUT_FILENAME_FREQ);
    fprintf(fid,"reset\n");
    fprintf(fid,"set terminal postscript eps enhanced color solid rounded\n");
    fprintf(fid,"set xrange [-0.5:0.5];\n");
    fprintf(fid,"set yrange [-100:10]\n");
    fprintf(fid,"set size ratio 0.6\n");
    fprintf(fid,"set xlabel 'Normalized Frequency'\n");
    fprintf(fid,"set ylabel 'Power Spectral Density [dB]'\n");
    fprintf(fid,"set nokey\n");
    fprintf(fid,"set xtics -0.5,0.1,0.5\n");
    fprintf(fid,"set ytics -200,20,200\n");
    fprintf(fid,"set grid xtics ytics\n");
    fprintf(fid,"set grid linetype 1 linecolor rgb '%s' lw 1\n", LIQUID_DOC_COLOR_GRID);

    fprintf(fid,"# spectrum\n");
    fprintf(fid,"set style fill solid 0.1\n");
    fprintf(fid,"plot '-' using 1:2 with filledcurves above y1=-200 linetype 1 linewidth 3 linecolor rgb '%s' title 'input'\n",LIQUID_DOC_COLOR_PURPLE);
    for (i=0; i<nfft; i++)
        fprintf(fid,"%12.8f %12.4e\n", (float)i/(float)nfft - 0.5f, 20*log10f(cabsf(Y[(i+nfft/2)%nfft])));
    fprintf(fid,"e\n");

    fclose(fid);
    printf("results written to '%s\n", OUTPUT_FILENAME_FREQ);

    return 0;
}
