/* COPYRIGHT (c) 2014 Umut Acar, Arthur Chargueraud, and Michael
 * Rainey
 * All rights reserved.
 *
 * \file bench.cpp
 * \brief Benchmarking driver
 *
 */

#include "benchmark.hpp"
#include "hash.hpp"
#include "dup.hpp"
#include "string.hpp"
#include "sort.hpp"
#include "graph.hpp"

/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Random-array generation */

loop_controller_type random_array_contr("random_array");

// returns a random array of size n using seed s
array random_array(long s, long n) {
  array tmp = array(n);
  par::parallel_for(random_array_contr, 0l, n, [&] (long i) {
    tmp[i] = hash_signed(i+s);
  });
  return tmp;
}

loop_controller_type almost_sorted_array_contr("almost_sorted_array");

// returns an array that is sorted up to a given number of swaps
array almost_sorted_array(long s, long n, long nb_swaps) {
  array tmp = array(n);
  par::parallel_for(almost_sorted_array_contr, 0l, n, [&] (long i) {
    tmp[i] = i;
  });
  for (long i = 0; i < nb_swaps; i++)
    std::swap(tmp[random_index(2*i, n)], tmp[random_index(2*i+1, n)]);
  return tmp;
}

loop_controller_type exp_dist_array_contr("exp_dist_array");

// returns an array with exponential distribution of size n using seed s
array exp_dist_array(long s, long n) {
  array tmp = array(n);
  int lg = log2_up(n)+1;
  par::parallel_for(exp_dist_array_contr, 0l, n, [&] (long i) {
    long range = (1 << (random_index(2*(i+s), lg)));
    tmp[i] = hash64shift((long)(range+random_index(2*(i+s), range)));
  });
  return tmp;
}

/*---------------------------------------------------------------------*/
/* Benchmark framework */

using thunk_type = std::function<void ()>;

using benchmark_type =
  std::pair<std::pair<thunk_type,thunk_type>,
            std::pair<thunk_type, thunk_type>>;

benchmark_type make_benchmark(thunk_type init, thunk_type bench,
                              thunk_type output, thunk_type destroy) {
  return std::make_pair(std::make_pair(init, bench),
                        std::make_pair(output, destroy));
}

void bench_init(const benchmark_type& b) {
  b.first.first();
}

void bench_run(const benchmark_type& b) {
  b.first.second();
}

void bench_output(const benchmark_type& b) {
  b.second.first();
}

void bench_destroy(const benchmark_type& b) {
  b.second.second();
}

/*---------------------------------------------------------------------*/
/* Benchmark definitions */


benchmark_type reduce_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  array_ptr inp = new array(0);
  value_type* result = new value_type;
  auto init = [=] {
    *inp = fill(n, 1);
  };
  auto bench = [=] {
    *result = sum(*inp);
  };
  auto output = [=] {
    std::cout << "result\t" << *result << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete result;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type scan_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  array_ptr inp = new array(0);
  array_ptr outp = new array(0);
  auto init = [=] {
    *inp = fill(n, 1);
  };
  auto bench = [=] {
    *outp = partial_sums(*inp).prefix;
  };
  auto output = [=] {
    std::cout << "result\t" << (*outp)[outp->size()-1] << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type sort_bench() {
  long n = pasl::util::cmdline::parse_or_default_long("n", 1l<<20);
  array_ptr inp = new array(0);
  array_ptr outp = new array(0);
  std::string s = pasl::util::cmdline::parse_string("algo");
  if (s != "quicksort" && s != "mergesort")
    pasl::util::atomic::fatal([&] { std::cerr << "bogus algo:" << s << std::endl; });
  auto sort_fct = (s == "quicksort")
    ? [] (array_ref xs) { return quicksort(xs); }
    : [] (array_ref xs) { return mergesort(xs); };
  auto init = [=] {
    pasl::util::cmdline::argmap_dispatch c;
    c.add("random", [&] {
      *inp = random_array(12345, n);
    });
    c.add("almost_sorted", [&] {
      long nb_swaps = pasl::util::cmdline::parse_or_default_long("nb_swaps", 1000);
      *inp = almost_sorted_array(1232, n, nb_swaps);
    });
    c.add("exponential_dist", [&] {
      *inp = exp_dist_array(12323, n);
    });
    c.find_by_arg_or_default_key("generator", "random")();
  };
  auto bench = [=] {
    *outp = sort_fct(*inp);
  };
  auto output = [=] {
    std::cout << "result\t" << (*outp)[outp->size()-1] << std::endl;
  };
  auto destroy = [=] {
    delete inp;
    delete outp;
  };
  return make_benchmark(init, bench, output, destroy);
}

benchmark_type graph_bench() {
  adjlist* graphp = new adjlist;
  array* distsp = new array;
  std::string fname = pasl::util::cmdline::parse_or_default_string("fname", "");
  vtxid_type source = pasl::util::cmdline::parse_or_default_long("source", 0l);
  if (fname == "")
    pasl::util::atomic::fatal([] { std::cerr << "missing filename for graph: -fname filename"; });
  auto init = [=] {
    graphp->load_from_file(fname);
  };
  auto bench = [=] {
    *distsp = bfs(*graphp, source);
  };
  auto output = [=] {
    long nb_visited = sum(map([] (value_type v) { return (v != dist_unknown); }, *distsp));
    long max_dist = max(*distsp);
    std::cout << "nb_visited\t" << nb_visited << std::endl;
    std::cout << "max_dist\t" << max_dist << std::endl;
  };
  auto destroy = [=] {
    delete graphp;
    delete distsp;
  };
  return make_benchmark(init, bench, output, destroy);
}

/*---------------------------------------------------------------------*/
/* PASL Driver */

int main(int argc, char** argv) {

  benchmark_type bench;
  
  auto init = [&] {
    pasl::util::cmdline::argmap<std::function<benchmark_type()>> m;
    m.add("reduce",      [&] { return reduce_bench(); });
    m.add("scan",        [&] { return scan_bench(); });
    m.add("sort",        [&] { return sort_bench(); });
    m.add("graph",       [&] { return graph_bench(); });
    bench = m.find_by_arg("bench")();
    bench_init(bench);
  };
  auto run = [&] (bool) {
    bench_run(bench);
  };
  auto output = [&] {
    bench_output(bench);
  };
  auto destroy = [&] {
    bench_destroy(bench);
  };
  pasl::sched::launch(argc, argv, init, run, output, destroy);
}

/***********************************************************************/

