#pragma once

#include "netcdf_input.hpp"
#include "step.hpp"

class WindFile : private netcdf_input::ParallelInputFile {
   public:
    WindFile(const Decomp2D& dec, const SimConfig& cfg);
    WindSample sample(double time);

   private:
    struct Record {
        MPI_Offset index = -1;
        WindTile tile;
    };
    void read(MPI_Offset index, Record& record);

    const Decomp2D& dec_;
    double interval_;
    int varids_[2]{};
    int ndims_ = 0;
    MPI_Offset records_ = 1;
    Record lower_, upper_;
};
