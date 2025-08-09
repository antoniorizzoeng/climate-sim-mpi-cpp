#include "io.hpp"
#include <fstream>

void write_field_csv(const Field& f, const std::string& filename) {
    std::ofstream ofs(filename);
    for (int j = 0; j < f.ny_total(); ++j) {
        for (int i = 0; i < f.nx_total(); ++i) {
            ofs << f.at(i,j);
            if (i < f.nx_total()-1) ofs << ",";
        }
        ofs << "\n";
    }
}
