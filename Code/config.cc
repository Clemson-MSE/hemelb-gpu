#include "config.h"

//I make a change here.
// the constants needed to define the configuration of the lattice
// sites follow

unsigned int SOLID_TYPE  = 0U;
unsigned int FLUID_TYPE  = 1U;
unsigned int INLET_TYPE  = 2U;
unsigned int OUTLET_TYPE = 3U;
unsigned int NULL_TYPE   = 4U;

unsigned int BOUNDARIES              = 4U;
unsigned int INLET_BOUNDARY          = 0U;
unsigned int OUTLET_BOUNDARY         = 1U;
unsigned int WALL_BOUNDARY           = 2U;
unsigned int CHARACTERISTIC_BOUNDARY = 3U;

unsigned int SITE_TYPE_BITS       = 2U;
unsigned int BOUNDARY_CONFIG_BITS = 14U;
unsigned int BOUNDARY_DIR_BITS    = 4U;
unsigned int BOUNDARY_ID_BITS     = 10U;

unsigned int BOUNDARY_CONFIG_SHIFT = 2U;   // SITE_TYPE_BITS;
unsigned int BOUNDARY_DIR_SHIFT    = 16U;  // BOUNDARY_CONFIG_SHIFT + BOUNDARY_CONFIG_BITS;
unsigned int BOUNDARY_ID_SHIFT     = 20U;  // BOUNDARY_DIR_SHIFT + BOUNDARY_DIR_BITS;

unsigned int SITE_TYPE_MASK       = ((1U <<  2U) - 1U);         // ((1U << SITE_TYPE_BITS) - 1U);
unsigned int BOUNDARY_CONFIG_MASK = ((1U << 14U) - 1U) << 2U;   // ((1U << BOUNDARY_CONFIG_BITS) - 1U) << BOUNDARY_CONFIG_SHIFT;
unsigned int BOUNDARY_DIR_MASK    = ((1U <<  4U) - 1U) << 16U;  //((1U << BOUNDARY_DIR_BITS) - 1U)    << BOUNDARY_DIR_SHIFT;
unsigned int BOUNDARY_ID_MASK     = ((1U << 10U) - 1U) << 20U;  // ((1U << BOUNDARY_ID_BITS) - 1U)     << BOUNDARY_ID_SHIFT
unsigned int PRESSURE_EDGE_MASK   = 1U << 31U;

unsigned int FLUID  = 1U;
unsigned int INLET  = 2U;
unsigned int OUTLET = 4U;
unsigned int EDGE   = 8U;


// square of the speed of sound

double Cs2 = 1.0 / 3.0;


// parameters related to the lattice directions

int e_x[] = { 0, 1,-1, 0, 0, 0, 0, 1,-1, 1,-1, 1,-1, 1,-1};
int e_y[] = { 0, 0, 0, 1,-1, 0, 0, 1,-1, 1,-1,-1, 1,-1, 1};
int e_z[] = { 0, 0, 0, 0, 0, 1,-1, 1,-1,-1, 1, 1,-1,-1, 1};
int inv_dir[] = {0, 2, 1, 4, 3, 6, 5, 8, 7, 10, 9, 12, 11, 14, 13};

#ifndef NOMPI
MPI_Datatype MPI_col_pixel_type;
#endif


#ifndef NO_STEER
pthread_mutex_t network_buffer_copy_lock;
pthread_mutex_t LOCK;
pthread_cond_t network_send_frame;
sem_t nrl;
sem_t connected_sem;
sem_t steering_var_lock;

bool is_frame_ready;
bool connected;
bool sending_frame;

int send_array_length;

int frame_size;
#endif


unsigned char *pixel_data = NULL;


double *f_old = NULL, *f_new = NULL;

int *f_id = NULL;

Cluster *cluster = NULL;

float **cluster_voxel = NULL;

float ***cluster_flow_field = NULL;


// 3 buffers needed for convergence-enabled simulations
double *f_to_send = NULL;
double *f_to_recv = NULL;

int *f_send_id = NULL;

int *f_recv_iv = NULL;

short int *f_data = NULL;

double *net_site_nor = NULL;
unsigned int *net_site_data = NULL;

double *inlet_density = NULL;
double *inlet_density_avg = NULL, *inlet_density_amp = NULL, *inlet_density_phs = NULL;
double *outlet_density = NULL;
double *outlet_density_avg = NULL, *outlet_density_amp = NULL, *outlet_density_phs = NULL;


int col_pixels, col_pixels_max;
int col_pixels_recv[2];

int *col_pixel_id = NULL;

ColPixel col_pixel_send[COLOURED_PIXELS_MAX];
ColPixel col_pixel_recv[2][COLOURED_PIXELS_MAX];

Glyph *glyph = NULL;


int is_bench;

// 3 variables needed for convergence-enabled simulations
double conv_error;
int cycle_tag, check_conv;
int is_inlet_normal_available;


int sites_x, sites_y, sites_z;
int blocks_x, blocks_y, blocks_z;
int blocks_yz, blocks;
int block_size, block_size2, block_size3, block_size_1;
int shift;
int sites_in_a_block;

double lbm_stress_type;
double lbm_stress_par;
double lbm_density_min, lbm_density_max;
double lbm_velocity_min, lbm_velocity_max;
double lbm_stress_min, lbm_stress_max;
double *lbm_average_inlet_velocity = NULL;
double *lbm_peak_inlet_velocity = NULL;
double *lbm_inlet_normal = NULL;
long int *lbm_inlet_count = NULL;

int lbm_terminate_simulation;

int net_machines;

int vis_period = 0;
int vis_image_freq = 0;
int vis_pixels_max = 0;
int vis_streaklines = 1;

float block_size_f;
float block_size_inv;
float vis_physical_pressure_threshold_min;
float vis_physical_pressure_threshold_max;
float vis_physical_velocity_threshold_max;
float vis_physical_stress_threshold_max;
float vis_density_threshold_min, vis_density_threshold_minmax_inv;
float vis_velocity_threshold_max_inv;
float vis_stress_threshold_max_inv;
float vis_brightness;
float vis_ctr_x, vis_ctr_y, vis_ctr_z;
double vis_glyph_length;
float vis_streaklines_per_pulsatile_period, vis_streakline_length;

int vis_mouse_x, vis_mouse_y;
int vis_perform_rendering;
int vis_mode;

int cluster_blocks_vec[3];
int cluster_blocks_z, cluster_blocks_yz, cluster_blocks;

simulationParameters simParams = simulationParameters();

float ray_dir[3];
float ray_inv[3];
float ray_vel_col[3];
float ray_stress_col[3];
float ray_length;
float ray_t_min;
float ray_density;
float ray_stress;

int clusters;


int glyphs;


Screen screen;

Viewpoint viewpoint;

Vis vis;


// some simple math functions

int min (int a, int b)
{
  if (a < b)
    {
      return a;
    }
  else
    {
      return b;
    }
}


int max (int a, int b)
{
  if (a > b)
    {
      return a;
    }
  else
    {
      return b;
    }
}


int nint (float a)
{
  if (a > (int)(a + 0.5F))
    {
      return 1 + (int)a;
    }
  else
    {
      return (int)a;
    }
}


double myClock ()
{
#ifdef NOMPI
  struct timeval time_data;
  
  gettimeofday (&time_data, NULL);
  
  return (double)time_data.tv_sec + (double)time_data.tv_usec / 1.e6;
#else
  return MPI_Wtime();
#endif
  
  //double time;
  //
  //int rc;
  //
  //struct timespec ts;
  //
  //
  //rc = clock_gettime (CLOCK_REALTIME, &ts);
  //
  //if (rc == 0)
  //  {
  //    time = (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.;
  //  }
  //else
  //  {
  //    fprintf(stderr, "ERROR: clock_gettime() failed\n");
  //    time = 0.;
  //  }
  //return time;
}


