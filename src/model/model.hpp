#pragma once


#include "io/io.hpp"
#include "io/python/io_python.hpp"
#include "parameters/parameters.hpp"
#include "tools/types.hpp"
#include "geometry/geometry.hpp"
#include "chemistry/chemistry.hpp"
#include "thermodynamics/thermodynamics.hpp"
#include "lines/lines.hpp"
#include "radiation/radiation.hpp"
#include <set>
#include "image/image.hpp"
#include "mgController/mgControllerHelper.hpp"

/// The main structure, which has a reference to most of the other structures
/////////////////////////////////////////////////////////////////////////////
struct Model
{
    bool writing_populations_to_disk=false;   ///< Toggle for writing the level populations after each iteration

    const Size MIN_INTERPOLATION_POINTS=16;   ///< Minimum number of points used during interpolation (~=average nb neighors in voronoi grid)

    const Size MAX_INTERPOLATION_POINTS=16;   ///< Maximum number of points used during interpolation (~=average nb neighors in voronoi grid)

    Size iteration_to_start_from=0;           ///< Number to start the iterations from (can be non-zero when loading the level populations)

    Parameters     parameters;
    Geometry       geometry;
    Chemistry      chemistry;
    Thermodynamics thermodynamics;
    Lines          lines;
    Radiation      radiation;
    vector<Image>  images;
    MgControllerHelper mgControllerHelper;


    vector<vector<VectorXr>> computed_level_populations;///< Vector of computed level populations, used for multigrid purposes. (coarsening level, line species, lineProducingSpecies.index(point, level))

    enum SpectralDiscretisation {None, SD_Lines, SD_Image}
         spectralDiscretisation = None;

    Model () {};
    Model (const string name)
    {
        parameters.model_name() = name;
        read ();
    }

    void read  (const Io& io);
    void write (const Io& io) const;

    inline double calc_diff_abundance_with_point(Size point1, Size point2);

    inline bool points_are_similar(Size point1, Size point2, double tolerance);

    inline double calc_distance2(Size point1,Size point2);

    inline void coarsen (double tol, Size new_coars_lvl);

    inline bool can_be_coarsened (const Size p, std::set<Size>& points_coarsened_around, double tol, Size new_coars_lvl);

    inline void coarsen_around_point (const Size p, Size new_coars_lvl);

    inline void interpolate_relative_differences_local(Size coarser_lvl, vector<VectorXr> &relative_difference_levelpopulations);

    inline void interpolate_levelpops_local(Size coarser_lvl);

    inline int setup_multigrid(Size max_coars_lvl, double tol, Size mgImplementation, Size max_n_iterations, Size finest_lvl);

    void read  ()       {read  (IoPython ("hdf5", parameters.model_name()));};
    void write () const {write (IoPython ("hdf5", parameters.model_name()));};

    int compute_inverse_line_widths               ();
    int compute_spectral_discretisation           ();
    int compute_spectral_discretisation           (
        const Real width );
    int compute_spectral_discretisation           (
        const long double nu_min,
        const long double nu_max );
    int compute_LTE_level_populations             ();
    int compute_radiation_field                   ();
    int compute_radiation_field_feautrier_order_2 ();
    int compute_radiation_field_shortchar_order_0 ();
    int compute_Jeff                              ();
    int compute_level_populations_from_stateq     ();
    int compute_level_populations                 (
        const bool  use_Ng_acceleration,
        const long  max_niterations);
    int compute_level_populations_multigrid       (
        const bool  use_Ng_acceleration);
    int compute_image                             (const Size ray_nr);

    int restart_from_iteration(Size iteration, Size lvl);

    Double1 error_max;
    Double1 error_mean;

    pc::multi_threading::ThreadPrivate<Vector<Real>> a;
    pc::multi_threading::ThreadPrivate<Vector<Real>> b;
    pc::multi_threading::ThreadPrivate<Vector<Real>> c;


    int set()
    {
       for (Size i = 0; i < pc::multi_threading::n_threads_avail(); i++)
       {
           const Size N = 1000000;

           cout << "Setting thread " << i << endl;

           a(i).resize(N);
           b(i).resize(N);
           c(i).resize(N);

           for (Size n = 0; n < N; n++)
           {
               a(i)[n] = 1.0;
               b(i)[n] = 2.0;
               c(i)[n] = 5.0;
           }
       }

       return (0);
    }

    Vector<Real> add ()
    {
        cout << "c.size() = " << c().vec.size() << endl;

        accelerated_for (i, c().vec.size(), nblocks, nthreads,
        {
            c()[i] = a()[i] + b()[i];

            // cout << "c[" << i << "] = " << c()[i] << endl;
        })

        return c().vec;
    }

    // Kernel approach
    Matrix<Real> eta;
    Matrix<Real> chi;

    Matrix<Real> boundary_condition;

    int set_eta_and_chi       ();
    int set_boundary_condition();
};

#include "model.tpp"
