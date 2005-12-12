#include <minc2.h>
#include <stdio.h>
#include <R.h>
#include <Rinternals.h>
#include <Rdefines.h>

/* compiling:
gcc -shared -fPIC -I/usr/lib/R/include/ -I/projects/mice/share/arch/linux64/include -L/projects/mice/share/arch/linux64/lib -o minc_reader.so minc_reader.c -lminc2 -lhdf5 -lnetcdf -lz -lm
*/

void get_volume_sizes(char **filename, unsigned int *sizes) {
  int result;
  mihandle_t  hvol;
  unsigned long tmp_sizes[3];
  midimhandle_t dimensions[3];
   /* open the existing volume */
  result = miopen_volume(filename[0],
			 MI2_OPEN_READ, &hvol);
  if (result != MI_NOERROR) {
    error("Error opening input file: %s.\n", filename[0]);
  }

    /* get the file dimensions and their sizes */
  miget_volume_dimensions( hvol, MI_DIMCLASS_SPATIAL,
			   MI_DIMATTR_ALL, MI_DIMORDER_FILE,
			   3, dimensions);
  result = miget_dimension_sizes( dimensions, 3, tmp_sizes ); 
  Rprintf("Sizes: %i %i %i\n", tmp_sizes[0], tmp_sizes[1], tmp_sizes[2]);
  sizes[0] = (unsigned int) tmp_sizes[0];
  sizes[1] = (unsigned int) tmp_sizes[1];
  sizes[2] = (unsigned int) tmp_sizes[2];
  return;
}

/* get a voxel from all files */
void get_voxel_from_files(char **filenames, int *num_files,
			  int *v1, int *v2, int *v3, double *voxel) {
  unsigned long location[3];
  mihandle_t hvol;
  int result;
  int i;

  location[0] = *v1;
  location[1] = *v2;
  location[2] = *v3;

  for(i=0; i < *num_files; i++) {
    /* open the volume */
    result = miopen_volume(filenames[i],
			   MI2_OPEN_READ, &hvol);
    if (result != MI_NOERROR) {
      error("Error opening input file: %s.\n", filenames[i]);
    }

    result = miget_real_value(hvol, location, 3, &voxel[i]);

    if (result != MI_NOERROR) {
      error("Error getting voxel from: %s.\n", filenames[i]);
    }
    miclose_volume(hvol);
  }
}
  

/* get a real value hyperslab from file */
void get_hyperslab(char **filename, int *start, int *count, double *slab) {
  int                result;
  mihandle_t         hvol;
  int                i;
  unsigned long      tmp_start[3];
  unsigned long      tmp_count[3];

  /* open the volume */
  result = miopen_volume(filename[0],
			 MI2_OPEN_READ, &hvol);
  if (result != MI_NOERROR) {
    error("Error opening input file: %s.\n", filename[0]);
  }

  for (i=0; i < 3; i++) {
    tmp_start[i] = (unsigned long) start[i];
    tmp_count[i] = (unsigned long) count[i];
  }

  /* get the hyperslab */
  Rprintf("Start: %i %i %i\n", start[0], start[1], start[2]);
  Rprintf("Count: %i %i %i\n", count[0], count[1], count[2]);
  if (miget_real_value_hyperslab(hvol, 
				 MI_TYPE_DOUBLE, 
				 (unsigned long *) tmp_start, 
				 (unsigned long *) tmp_count, 
				 slab)
      < 0) {
    error("Could not get hyperslab.\n");
  }
  return;
}

/* get a real value hyperslab from file */
SEXP get_hyperslab2( SEXP filename,  SEXP start,  SEXP count, SEXP slab) {
  int                result;
  mihandle_t         hvol;
  int                i;
  unsigned long      tmp_start[3];
  unsigned long      tmp_count[3];

  /*
  char **c_filename;
  int *c_start;
  int *c_count;
  double *c_slab;
  */
  /* open the volume */
  
  Rprintf("Crap %s\n", CHAR(STRING_ELT(filename, 0)));
  result = miopen_volume(CHAR(STRING_ELT(filename,0)),
			 MI2_OPEN_READ, &hvol);
  if (result != MI_NOERROR) {
    error("Error opening input file: %s.\n", CHAR(STRING_ELT(filename,0)));
  }

  for (i=0; i < 3; i++) {
    tmp_start[i] = (unsigned long) INTEGER(start)[i];
    tmp_count[i] = (unsigned long) INTEGER(count)[i];
  }

  /* get the hyperslab */
  Rprintf("Start: %i %i %i\n", INTEGER(start)[0], INTEGER(start)[1], INTEGER(start)[2]);
  Rprintf("Count: %i %i %i\n", INTEGER(count)[0], INTEGER(count)[1], INTEGER(count)[2]);
  if (miget_real_value_hyperslab(hvol, 
				 MI_TYPE_DOUBLE, 
				 (unsigned long *) tmp_start, 
				 (unsigned long *) tmp_count, 
				 REAL(slab))
      < 0) {
    error("Could not get hyperslab.\n");
  }
  return(slab);
}

/* minc2_apply: evaluate a function at every voxel of a series of files
 * filenames: character array of filenames. Have to have identical sampling.
 * fn: string representing a function call to be evaluated. The variable "x"
 *     will be a vector of length number_volumes in the same order as the 
 *     filenames array.
 * rho: the R environment.
 */
     
SEXP minc2_apply(SEXP filenames, SEXP fn, SEXP rho) {
  int                result;
  mihandle_t         *hvol;
  int                i, v0, v1, v2, index;
  unsigned long      start[3];
  unsigned long      location[3];
  int                num_files;
  double             *xbuffer, *xoutput, **full_buffer;
  midimhandle_t      dimensions[3];
  unsigned long      sizes[3];
  SEXP               output, buffer, R_fcall;
  

  /* allocate memory for volume handles */
  num_files = LENGTH(filenames);
  hvol = malloc(num_files * sizeof(mihandle_t));

  Rprintf("Number of volumes: %i\n", num_files);

  /* open each volume */
  for(i=0; i < num_files; i++) {
    result = miopen_volume(CHAR(STRING_ELT(filenames, i)),
      MI2_OPEN_READ, &hvol[i]);
    if (result != MI_NOERROR) {
      error("Error opening input file: %s.\n", CHAR(STRING_ELT(filenames,i)));
    }
  }

  /* get the file dimensions and their sizes - assume they are the same*/
  miget_volume_dimensions( hvol[0], MI_DIMCLASS_SPATIAL,
			   MI_DIMATTR_ALL, MI_DIMORDER_FILE,
			   3, dimensions);
  result = miget_dimension_sizes( dimensions, 3, sizes );
  Rprintf("Volume sizes: %i %i %i\n", sizes[0], sizes[1], sizes[2]);

  /* allocate the output buffer */
  PROTECT(output=allocVector(REALSXP, (sizes[0] * sizes[1] * sizes[2])));
  xoutput = REAL(output);

  /* allocate the local buffer that will be passed to the function */
  PROTECT(buffer=allocVector(REALSXP, num_files));
  xbuffer = REAL(buffer); 

  //PROTECT(R_fcall = lang2(fn, R_NilValue));


  /* allocate the buffer */
  start[0] = 0; start[1] = 0; start[2] = 0;
  full_buffer = malloc(num_files * sizeof(double));

  /* fill the buffer */
  for (i=0; i < num_files; i++) {
    full_buffer[i] = malloc(sizes[0] * sizes[1] * sizes[2] * sizeof(double));
    if (miget_real_value_hyperslab(hvol[i], 
				   MI_TYPE_DOUBLE, 
				   (unsigned long *) start, 
				   (unsigned long *) sizes, 
				   full_buffer[i]) )
	error("Error opening buffer.\n");
  }
	

  /* loop across all files and voxels */
  Rprintf("In slice \n");
  for (v0=0; v0 < sizes[0]; v0++) {
    Rprintf(" %d ", v0);
    for (v1=0; v1 < sizes[1]; v1++) {
      for (v2=0; v2 < sizes[2]; v2++) {
	index = v0*sizes[1]*sizes[2]+v1*sizes[2]+v2;

	for (i=0; i < num_files; i++) {
	  location[0] = v0;
	  location[1] = v1;
	  location[2] = v2;
	  //SET_VECTOR_ELT(buffer, i, full_buffer[i][index]);
	  //result = miget_real_value(hvol[i], location, 3, &xbuffer[i]);
	  xbuffer[i] = full_buffer[i][index];
	  
	  //Rprintf("V%i: %f\n", i, full_buffer[i][index]);

	}
	/* install the variable "x" into environment */
	defineVar(install("x"), buffer, rho);
	//SETCADDR(R_fcall, buffer);
	//SET_VECTOR_ELT(output, index, eval(R_fcall, rho));
	//SET_VECTOR_ELT(output, index, test);
	/* evaluate the function */
	xoutput[index] = REAL(eval(fn, rho))[0]; 
      }
    }
  }
  Rprintf("\nDone\n");

  /* free memory */
  for (i=0; i<num_files; i++) {
    miclose_volume(hvol[i]);
    free(full_buffer[i]);
  }
  free(full_buffer);
  UNPROTECT(3);

  /* return the results */
  return(output);
}

/* writes a hyperslab to the output filename, creating the output voluem
   to be like the second filename passed in */
void write_minc2_volume(char **output, char **like_filename,
			int *start, int *count, double *max_range,
			double *min_range, double *slab) {
  mihandle_t hvol_like, hvol_new;
  midimhandle_t dimensions_like[3], dimensions_new[3];
  unsigned long tmp_count[3];
  unsigned long tmp_start[3];
  mivolumeprops_t props;
  int i;

  /* read the like volume */
  if (miopen_volume(like_filename[0], MI2_OPEN_READ, &hvol_like) < 0 ) {
    error("Error opening volume: %s\n", like_filename[0]);
  }
  
  /* get dimensions */
  miget_volume_dimensions( hvol_like, MI_DIMCLASS_SPATIAL, MI_DIMATTR_ALL,
			   MI_DIMORDER_FILE, 3, dimensions_like );

  /* copy the dimensions to the new file */
  for (i=0; i < 3; i++) {
    micopy_dimension(dimensions_like[i], &dimensions_new[i]);
  }

  /* create the new volume */
  if ( micreate_volume(output[0], 3, dimensions_new, MI_TYPE_USHORT,
		       MI_CLASS_REAL, NULL, &hvol_new) < 0 ) {
    error("Error creating volume %s\n", output[0]);
  }
  if (micreate_volume_image(hvol_new) < 0) {
    error("Error creating volume image\n");
  }
  
  /* set valid and real range */
  miset_volume_valid_range(hvol_new, 65535, 0);
  miset_volume_range(hvol_new, max_range[0], min_range[0]);

  Rprintf("Range: %f %f\n", max_range[0], min_range[0]);

  /* write the buffer */
  for (i=0; i < 3; i++) {
    tmp_start[i] = (unsigned long) start[i];
    tmp_count[i] = (unsigned long) count[i];
  }
  if (miset_real_value_hyperslab(hvol_new, MI_TYPE_DOUBLE, 
				 (unsigned long *) tmp_start, 
				 (unsigned long *) tmp_count,
				 slab) < 0) {
    error("Error writing buffer to volume\n");
  }
  miclose_volume(hvol_like);
  miclose_volume(hvol_new);
  return;
}
