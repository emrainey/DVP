/* ====================================================================
 *   Copyright (C) 2012 Texas Instruments Incorporated
 * ==================================================================== */

/*!
 * \file
 * \brief This file decribes the exact set of kernels supported by the DSPLIB
 * algorithm library as exported through DVP.
 * \note When using these enums, only the DSPLIB kernels will be used!
 * \defgroup group_algo_dsplib Algos: DSP Library
 */

#ifndef _DVP_KL_DSPLIB_H_
#define _DVP_KL_DSPLIB_H_

#ifdef DVP_USE_DSPLIB

#include <dvp/dvp_types.h>
//#define DVP_DSPLIB_TEST1 1
//#define DVP_DSPLIB_TEST2 1

/*!
 * \brief The explicit list of kernels supported by DSPLIB.
 * \see DVP_DSPFunc
 * \ingroup group_algo_dsplib
 */
enum {
        DVP_KN_DSP_BASE = DVP_KN_LIBRARY_BASE(DVP_KL_DSPLIB),
        DVP_KN_DSP_ADD16,       //!< 16-bit Addition
        DVP_KN_DSP_ADD32,       //!< 32-bit Addition
        DVP_KN_DSP_AUTOCORR16,  //!< Autocorrelation
        DVP_KN_DSP_BITEXP32,    //!< Block exponent
        DVP_KN_DSP_BLKESWAP16,  //!< Endian-swap a block of 16-bit values
        DVP_KN_DSP_BLKESWAP32,  //!< Endian-swap a block of 32-bit values
        DVP_KN_DSP_BLKESWAP64,  //!< Endian-swap a block of 64-bit values
        DVP_KN_DSP_BLKMOVE,     //!< Block Move
        DVP_KN_DSP_DOTPRODSQR,  //!< Vector dot product and square
        DVP_KN_DSP_DOTPROD,     //!< Vector dot product
        DVP_KN_DSP_FFT_16x16,   //!< Complex forward FFT with digital reversal
        DVP_KN_DSP_FFT_16x16R,  //!< Complex forward mixed radix 16- x 16-bit FFT with rounding
        DVP_KN_DSP_FFT_16x16_IMRE,
        DVP_KN_DSP_FFT_16x32,   //!< Complex forward mixed radix 16- x 32-bit FFT with rounding
        DVP_KN_DSP_FFT_32x32,   //!< Complex forward mixed radix 32- x 32-bit FFT with rounding
        DVP_KN_DSP_FFT_32x32s,  //!< Complex forward mixed radix 32- x 32-bit FFT with scaling
        DVP_KN_DSP_FIR_CPLX,    //!< Complex FIR filter (radix 2)
        DVP_KN_DSP_FIR_HM4x4,   //!< Complex FIR output with complex filter co-efficients (multiple of 4).
        DVP_KN_DSP_FIR_GEN,     //!< Real FIR fitering
        DVP_KN_DSP_FIR_HM17_RA8x8,
        DVP_KN_DSP_FIR_LMS2,    //!< LMS Adaptive Filtering
        DVP_KN_DSP_FIR_R4,      //!< Radix-4 FIR Filter
        DVP_KN_DSP_FIR_R8,      //!< Radix-8 FIR Filter
        DVP_KN_DSP_FIR_HM16_RM8A8x8,
        DVP_KN_DSP_FIR_SYM,     //!< Symmetric FIR filter (radix-8)
        DVP_KN_DSP_IFFT_16x16,  //!< Complex inverse mixed radix 16 x 16-bit FFT with rounding
        DVP_KN_DSP_IFFT_16x16_IMRE, //!< Similar to IFFT16.16 with reversed positions for real and imaginary components in signals
        DVP_KN_DSP_IFFT_16x32,  //!<  Complex inverse mixed radix 16 x 32-bit FFT with rounding
        DVP_KN_DSP_IFFT_32x32,  //!< Complex inverse mixed radix 32 x 32-bit FFT with rounding
        DVP_KN_DSP_IIR,         //!< IIR Filter with 5 coefficients
        DVP_KN_DSP_IIR_LAT,     //!< All-Pole IIR Lattice Filter
        DVP_KN_DSP_IIR_SS,      //!< Single-sample IIR filter
        DVP_KN_DSP_MUL,         //!< Matrix Multiplication
        DVP_KN_DSP_MUL_CPLX,    //!< Matrix Mulitplication of Complex Numbers
        DVP_KN_DSP_MAT_TRANS,   //!< Matrix Transpose
        DVP_KN_DSP_MAXIDX,      //!< Index of the maximum element of a vector
        DVP_KN_DSP_MAXVAL,      //!< Maximum value of a vector
        DVP_KN_DSP_MINERROR,    //!< Minimum energy error search
        DVP_KN_DSP_MINVAL,      //!< Minimum value of a vector.
        DVP_KN_DSP_MUL32,       //!< 32-bit vector multiply
        DVP_KN_DSP_NEG32,       //!< 32-bit vector negate
        DVP_KN_DSP_Q15TOFL,     //!< Q15 to float conversion --> Not supported on C64+ Fixed point DSP
        DVP_KN_DSP_RECIP16,     //!< 16-bit reciprocal
        DVP_KN_DSP_VECSUMSQ,    //!< Sum of squares
        DVP_KN_DSP_W_VEC,       //!< Weighted vector sum
        DVP_KN_GEN_TWIDDLE_FFT_16X16,   //!< Generate 16x16 twiddle factors needed for FFT operations
        DVP_KN_GEN_TWIDDLE_FFT_16X16_IMRE,  //!< Generate 16x16 twiddle factors needed for FFT IMRE operations
        DVP_KN_GEN_TWIDDLE_FFT_16X32,   //!< Generate 16x32 twiddle factors needed for FFT operations
        DVP_KN_GEN_TWIDDLE_FFT_32x32,   //!< Generate 32x32 twiddle factors needed for FFT operations
        DVP_KN_GEN_TWIDDLE_IFFT_16X16,  //!< Generate 16x16 twiddle factors needed for IFFT operations
        DVP_KN_GEN_TWIDDLE_IFFT_16X16_IMRE  //!< Generate 16x16 twiddle factors needed for IFFT IMRE operations
};

/*! \brief This is not a generic function. It can find if x is the power of no.
 * \note No has to be multiple of 2 and 4.
 * \param [in] x The value to determine.
 * \param [in] no The
 * \ingroup group_algo_dsplib
 */
static inline DVP_U16 isPowerOf (DVP_U32 x, DVP_U16  no )
{
    if ( no == 2)
    {
        return (
       x == 8 || x == 16 || x == 32 ||
       x == 64 || x == 128 || x == 256 || x == 512 || x == 1024 ||
       x == 2048 || x == 4096 || x == 8192 || x == 16384) ;
    }
    if ( no == 4)
    {
        return(x == 16 || x == 64 || x == 256 || x == 1024 ||
        x == 4096 || x == 16384 ) ;
    }
    else
         return 0;
}

/*! \brief Multiplication strucutre
 * \ingroup group_algo_dsplib
 */
typedef struct  _dsp_matmul{
    DVP_Buffer_t input0;
    DVP_Buffer_t input1;
    DVP_Buffer_t output0;
    DVP_S32 r1; // Rows of input0
    DVP_S32 c1; // Coloumns of input0
    DVP_S32 r2; // Rows of input1
    DVP_S32 c2; // Coloumns of input0
    DVP_S32 qs; // User defined right shift on final result.
}DVP_DSP_MatMul;

/*! \brief Generic Function Parameters to DSPLIB functions.
 * \ingroup group_algo_dsplib
 */
typedef struct  _dsp_func{
    DVP_Buffer_t input0;
    DVP_Buffer_t input1;
    DVP_Buffer_t twoway;
    DVP_Buffer_t output0;
    DVP_Buffer_t output1;
    DVP_S32 val1;
    DVP_S32 val2;
    DVP_S32 val3;
}DVP_DSPFunc;

#endif // DVP_USE_DSPLIB

#endif // _DVP_KL_DSPLIB_H_

