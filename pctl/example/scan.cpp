/*!
 * \file max.cpp
 * \brief Benchmarking script for parallel sorting algorithms
 * \date 2015
 * \copyright COPYRIGHT (c) 2015 Umut Acar, Arthur Chargueraud, and
 * Michael Rainey. All rights reserved.
 * \license This project is released under the GNU Public License.
 *
 */

#include "pasl.hpp"
#include "io.hpp"
#include "dpsdatapar.hpp"

/***********************************************************************/

namespace pasl {
  namespace pctl {
   
    void ex() {
      
      auto combine = [&] (long x, long y) {
        return x + y;
      };
      
      {
        parray<long> xs = { 1, 3, 9, 0, 33, 1, 1 };
        std::cout << "xs\t= " << xs << std::endl;
        
        parray<long> ys1 = scan(xs.cbegin(), xs.cend(), 0L, combine, forward_exclusive_scan);
        std::cout << "ys1\t= " << ys1 << std::endl;
        
        parray<long> ys2 = scan(xs.cbegin(), xs.cend(), 0L, combine, backward_exclusive_scan);
        std::cout << "ys2\t= " << ys2 << std::endl;
        
        parray<long> ys3 = scan(xs.cbegin(), xs.cend(), 0L, combine, forward_inclusive_scan);
        std::cout << "ys3\t= " << ys3 << std::endl;
        
        parray<long> ys4 = scan(xs.cbegin(), xs.cend(), 0L, combine, backward_inclusive_scan);
        std::cout << "ys4\t= " << ys4 << std::endl;
        
        // in-place scan
        
        long id = 0;
        dps::scan(xs.begin(), xs.end(), combine, id, xs.begin(), forward_exclusive_scan);
        std::cout << "xs\t= " << xs << std::endl;
      }
      
    }
    
  }
}

/*---------------------------------------------------------------------*/

int main(int argc, char** argv) {
  pasl::sched::launch(argc, argv, [&] (bool sequential) {
    pasl::pctl::ex();
  });
  return 0;
}

/***********************************************************************/
