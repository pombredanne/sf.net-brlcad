#ifdef cl_khr_fp64
    #pragma OPENCL EXTENSION cl_khr_fp64 : enable
#elif defined(cl_amd_fp64)
    #pragma OPENCL EXTENSION cl_amd_fp64 : enable
#else
    #error "Double precision floating point not supported by OpenCL implementation."
#endif

#define RT_PCOEF_TOL            (1.0e-10)
#define RT_DOT_TOL              (0.001)
#define RT_PCOEF_TOL		(1.0e-10)

#define SMALL_FASTF		(1.0e-77)
#define SQRT_MAX_FASTF		(1.0e36)	/* This squared just avoids overflow */
#define SQRT_SMALL_FASTF	(1.0e-39)	/* This squared gives zero */

#define M_PI_3			(1.04719755119659774615421446109316763)   /**< pi/3 */
#define M_SQRT3			(1.73205080756887729352744634150587237)   /**< sqrt(3) */

#define NEAR_ZERO(val, epsilon)	(((val) > -epsilon) && ((val) < epsilon))
#define ZERO(_a)        	NEAR_ZERO((_a), SMALL_FASTF)


struct hit {
  double3 hit_vpriv;
  double hit_dist;
  int hit_surfno;
};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

