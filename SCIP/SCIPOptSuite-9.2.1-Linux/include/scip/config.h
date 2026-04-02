#ifndef __CONFIG_H__
#define __CONFIG_H__

#define SCIP_BUILD_TYPE "Release"
#define SCIP_VERSION_MAJOR 10
#define SCIP_VERSION_MINOR 0
#define SCIP_VERSION_PATCH 0
#define SCIP_VERSION_API 156
/* #undef BMS_NOBLOCKMEM */
/* #undef SCIP_NOBUFFERMEM */
/* #undef WITH_DEBUG_SOLUTION */
/* #undef SCIP_NO_SIGACTION */
/* #undef SCIP_NO_STRTOK_R */
/* #undef TPI_NONE */
#define TPI_TNY
/* #undef TPI_OMP */
#define SCIP_THREADSAFE
#define WITH_SCIPDEF
/* #undef SCIP_WITH_LAPACK */
#define SCIP_WITH_PAPILO
#define SCIP_WITH_ZLIB
#define SCIP_WITH_READLINE
#define SCIP_WITH_GMP
#define SCIP_WITH_MPFR
/* #undef SCIP_WITH_QSOPTEX */
/* #undef SCIP_WITH_LPSCHECK */
/* #undef SCIP_WITH_ZIMPL */
#define SCIP_WITH_AMPL
#define SCIP_ROUNDING_FE
/* #undef SCIP_ROUNDING_FP */
/* #undef SCIP_ROUNDING_MS */
#define SCIP_WITH_EXACTSOLVE
/* #undef SCIP_WITH_EGLIB */
#define SCIP_WITH_BOOST
/* #undef SCIP_CHECK_STAGE */

#endif
