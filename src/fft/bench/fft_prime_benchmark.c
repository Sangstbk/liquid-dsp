/*
 * Copyright (c) 2007, 2009, 2012, 2013 Joseph Gaeddert
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
// fft_prime_benchmark.c : benchmark FFTs of prime length
//

#include <stdlib.h>
#include <stdio.h>
#include <sys/resource.h>
#include "liquid.h"

#include "src/fft/bench/fft_runbench.h"

// prime numbers
void benchmark_fft_3      LIQUID_FFT_BENCHMARK_API(     3, LIQUID_FFT_FORWARD)
void benchmark_fft_5      LIQUID_FFT_BENCHMARK_API(     5, LIQUID_FFT_FORWARD)
void benchmark_fft_7      LIQUID_FFT_BENCHMARK_API(     7, LIQUID_FFT_FORWARD)
void benchmark_fft_11     LIQUID_FFT_BENCHMARK_API(    11, LIQUID_FFT_FORWARD)
void benchmark_fft_13     LIQUID_FFT_BENCHMARK_API(    13, LIQUID_FFT_FORWARD)
void benchmark_fft_17     LIQUID_FFT_BENCHMARK_API(    17, LIQUID_FFT_FORWARD)
void benchmark_fft_19     LIQUID_FFT_BENCHMARK_API(    19, LIQUID_FFT_FORWARD)
void benchmark_fft_23     LIQUID_FFT_BENCHMARK_API(    23, LIQUID_FFT_FORWARD)
void benchmark_fft_29     LIQUID_FFT_BENCHMARK_API(    29, LIQUID_FFT_FORWARD)
void benchmark_fft_31     LIQUID_FFT_BENCHMARK_API(    31, LIQUID_FFT_FORWARD)
void benchmark_fft_37     LIQUID_FFT_BENCHMARK_API(    37, LIQUID_FFT_FORWARD)
void benchmark_fft_41     LIQUID_FFT_BENCHMARK_API(    41, LIQUID_FFT_FORWARD)
void benchmark_fft_43     LIQUID_FFT_BENCHMARK_API(    43, LIQUID_FFT_FORWARD)
void benchmark_fft_47     LIQUID_FFT_BENCHMARK_API(    47, LIQUID_FFT_FORWARD)
void benchmark_fft_53     LIQUID_FFT_BENCHMARK_API(    53, LIQUID_FFT_FORWARD)
void benchmark_fft_59     LIQUID_FFT_BENCHMARK_API(    59, LIQUID_FFT_FORWARD)
void benchmark_fft_61     LIQUID_FFT_BENCHMARK_API(    61, LIQUID_FFT_FORWARD)
void benchmark_fft_67     LIQUID_FFT_BENCHMARK_API(    67, LIQUID_FFT_FORWARD)
void benchmark_fft_71     LIQUID_FFT_BENCHMARK_API(    71, LIQUID_FFT_FORWARD)
void benchmark_fft_73     LIQUID_FFT_BENCHMARK_API(    73, LIQUID_FFT_FORWARD)
void benchmark_fft_79     LIQUID_FFT_BENCHMARK_API(    79, LIQUID_FFT_FORWARD)
void benchmark_fft_83     LIQUID_FFT_BENCHMARK_API(    83, LIQUID_FFT_FORWARD)
void benchmark_fft_89     LIQUID_FFT_BENCHMARK_API(    89, LIQUID_FFT_FORWARD)
void benchmark_fft_97     LIQUID_FFT_BENCHMARK_API(    97, LIQUID_FFT_FORWARD)
void benchmark_fft_101    LIQUID_FFT_BENCHMARK_API(   101, LIQUID_FFT_FORWARD)
void benchmark_fft_103    LIQUID_FFT_BENCHMARK_API(   103, LIQUID_FFT_FORWARD)
void benchmark_fft_107    LIQUID_FFT_BENCHMARK_API(   107, LIQUID_FFT_FORWARD)
void benchmark_fft_109    LIQUID_FFT_BENCHMARK_API(   109, LIQUID_FFT_FORWARD)
void benchmark_fft_113    LIQUID_FFT_BENCHMARK_API(   113, LIQUID_FFT_FORWARD)
void benchmark_fft_127    LIQUID_FFT_BENCHMARK_API(   127, LIQUID_FFT_FORWARD)
void benchmark_fft_131    LIQUID_FFT_BENCHMARK_API(   131, LIQUID_FFT_FORWARD)
void benchmark_fft_137    LIQUID_FFT_BENCHMARK_API(   137, LIQUID_FFT_FORWARD)
void benchmark_fft_139    LIQUID_FFT_BENCHMARK_API(   139, LIQUID_FFT_FORWARD)
void benchmark_fft_149    LIQUID_FFT_BENCHMARK_API(   149, LIQUID_FFT_FORWARD)
void benchmark_fft_151    LIQUID_FFT_BENCHMARK_API(   151, LIQUID_FFT_FORWARD)
void benchmark_fft_157    LIQUID_FFT_BENCHMARK_API(   157, LIQUID_FFT_FORWARD)
void benchmark_fft_163    LIQUID_FFT_BENCHMARK_API(   163, LIQUID_FFT_FORWARD)
void benchmark_fft_167    LIQUID_FFT_BENCHMARK_API(   167, LIQUID_FFT_FORWARD)
void benchmark_fft_173    LIQUID_FFT_BENCHMARK_API(   173, LIQUID_FFT_FORWARD)
void benchmark_fft_179    LIQUID_FFT_BENCHMARK_API(   179, LIQUID_FFT_FORWARD)
void benchmark_fft_181    LIQUID_FFT_BENCHMARK_API(   181, LIQUID_FFT_FORWARD)
void benchmark_fft_191    LIQUID_FFT_BENCHMARK_API(   191, LIQUID_FFT_FORWARD)
void benchmark_fft_193    LIQUID_FFT_BENCHMARK_API(   193, LIQUID_FFT_FORWARD)
void benchmark_fft_197    LIQUID_FFT_BENCHMARK_API(   197, LIQUID_FFT_FORWARD)
void benchmark_fft_199    LIQUID_FFT_BENCHMARK_API(   199, LIQUID_FFT_FORWARD)
void benchmark_fft_211    LIQUID_FFT_BENCHMARK_API(   211, LIQUID_FFT_FORWARD)
void benchmark_fft_223    LIQUID_FFT_BENCHMARK_API(   223, LIQUID_FFT_FORWARD)
void benchmark_fft_227    LIQUID_FFT_BENCHMARK_API(   227, LIQUID_FFT_FORWARD)
void benchmark_fft_229    LIQUID_FFT_BENCHMARK_API(   229, LIQUID_FFT_FORWARD)
void benchmark_fft_233    LIQUID_FFT_BENCHMARK_API(   233, LIQUID_FFT_FORWARD)
void benchmark_fft_239    LIQUID_FFT_BENCHMARK_API(   239, LIQUID_FFT_FORWARD)
void benchmark_fft_241    LIQUID_FFT_BENCHMARK_API(   241, LIQUID_FFT_FORWARD)
void benchmark_fft_251    LIQUID_FFT_BENCHMARK_API(   251, LIQUID_FFT_FORWARD)
void benchmark_fft_257    LIQUID_FFT_BENCHMARK_API(   257, LIQUID_FFT_FORWARD)
void benchmark_fft_263    LIQUID_FFT_BENCHMARK_API(   263, LIQUID_FFT_FORWARD)
void benchmark_fft_269    LIQUID_FFT_BENCHMARK_API(   269, LIQUID_FFT_FORWARD)
void benchmark_fft_271    LIQUID_FFT_BENCHMARK_API(   271, LIQUID_FFT_FORWARD)
void benchmark_fft_277    LIQUID_FFT_BENCHMARK_API(   277, LIQUID_FFT_FORWARD)
void benchmark_fft_281    LIQUID_FFT_BENCHMARK_API(   281, LIQUID_FFT_FORWARD)
void benchmark_fft_283    LIQUID_FFT_BENCHMARK_API(   283, LIQUID_FFT_FORWARD)
void benchmark_fft_293    LIQUID_FFT_BENCHMARK_API(   293, LIQUID_FFT_FORWARD)
void benchmark_fft_307    LIQUID_FFT_BENCHMARK_API(   307, LIQUID_FFT_FORWARD)
void benchmark_fft_311    LIQUID_FFT_BENCHMARK_API(   311, LIQUID_FFT_FORWARD)
void benchmark_fft_313    LIQUID_FFT_BENCHMARK_API(   313, LIQUID_FFT_FORWARD)
void benchmark_fft_317    LIQUID_FFT_BENCHMARK_API(   317, LIQUID_FFT_FORWARD)
void benchmark_fft_331    LIQUID_FFT_BENCHMARK_API(   331, LIQUID_FFT_FORWARD)
void benchmark_fft_337    LIQUID_FFT_BENCHMARK_API(   337, LIQUID_FFT_FORWARD)
void benchmark_fft_347    LIQUID_FFT_BENCHMARK_API(   347, LIQUID_FFT_FORWARD)
void benchmark_fft_349    LIQUID_FFT_BENCHMARK_API(   349, LIQUID_FFT_FORWARD)
void benchmark_fft_353    LIQUID_FFT_BENCHMARK_API(   353, LIQUID_FFT_FORWARD)
void benchmark_fft_359    LIQUID_FFT_BENCHMARK_API(   359, LIQUID_FFT_FORWARD)
void benchmark_fft_367    LIQUID_FFT_BENCHMARK_API(   367, LIQUID_FFT_FORWARD)
void benchmark_fft_373    LIQUID_FFT_BENCHMARK_API(   373, LIQUID_FFT_FORWARD)
void benchmark_fft_379    LIQUID_FFT_BENCHMARK_API(   379, LIQUID_FFT_FORWARD)
void benchmark_fft_383    LIQUID_FFT_BENCHMARK_API(   383, LIQUID_FFT_FORWARD)
void benchmark_fft_389    LIQUID_FFT_BENCHMARK_API(   389, LIQUID_FFT_FORWARD)
void benchmark_fft_397    LIQUID_FFT_BENCHMARK_API(   397, LIQUID_FFT_FORWARD)
void benchmark_fft_401    LIQUID_FFT_BENCHMARK_API(   401, LIQUID_FFT_FORWARD)
void benchmark_fft_409    LIQUID_FFT_BENCHMARK_API(   409, LIQUID_FFT_FORWARD)
void benchmark_fft_419    LIQUID_FFT_BENCHMARK_API(   419, LIQUID_FFT_FORWARD)
void benchmark_fft_421    LIQUID_FFT_BENCHMARK_API(   421, LIQUID_FFT_FORWARD)
void benchmark_fft_431    LIQUID_FFT_BENCHMARK_API(   431, LIQUID_FFT_FORWARD)
void benchmark_fft_433    LIQUID_FFT_BENCHMARK_API(   433, LIQUID_FFT_FORWARD)
void benchmark_fft_439    LIQUID_FFT_BENCHMARK_API(   439, LIQUID_FFT_FORWARD)
void benchmark_fft_443    LIQUID_FFT_BENCHMARK_API(   443, LIQUID_FFT_FORWARD)
void benchmark_fft_449    LIQUID_FFT_BENCHMARK_API(   449, LIQUID_FFT_FORWARD)
void benchmark_fft_457    LIQUID_FFT_BENCHMARK_API(   457, LIQUID_FFT_FORWARD)
void benchmark_fft_461    LIQUID_FFT_BENCHMARK_API(   461, LIQUID_FFT_FORWARD)
void benchmark_fft_463    LIQUID_FFT_BENCHMARK_API(   463, LIQUID_FFT_FORWARD)
void benchmark_fft_467    LIQUID_FFT_BENCHMARK_API(   467, LIQUID_FFT_FORWARD)
void benchmark_fft_479    LIQUID_FFT_BENCHMARK_API(   479, LIQUID_FFT_FORWARD)
void benchmark_fft_487    LIQUID_FFT_BENCHMARK_API(   487, LIQUID_FFT_FORWARD)
void benchmark_fft_491    LIQUID_FFT_BENCHMARK_API(   491, LIQUID_FFT_FORWARD)
void benchmark_fft_499    LIQUID_FFT_BENCHMARK_API(   499, LIQUID_FFT_FORWARD)
void benchmark_fft_503    LIQUID_FFT_BENCHMARK_API(   503, LIQUID_FFT_FORWARD)
void benchmark_fft_509    LIQUID_FFT_BENCHMARK_API(   509, LIQUID_FFT_FORWARD)

