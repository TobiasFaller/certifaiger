#pragma once
#include <cassert>
#include <iostream>
#include <numeric>
#include <ranges>
#include <span>
#include <vector>

// Wrapper around the aiger library.
extern "C" {
#include "aiger.h"
}

static constexpr unsigned INVALID_LIT = std::numeric_limits<unsigned>::max();

// I don't want to un-const the input circuits, so I'm going to use a const_cast
bool is_input(const aiger *aig, unsigned l) {
  return aiger_is_input(const_cast<aiger *>(aig), l);
}
bool is_latch(const aiger *aig, unsigned l) {
  return aiger_is_latch(const_cast<aiger *>(aig), l);
}
unsigned reset(const aiger *aig, unsigned l) {
  assert(is_latch(aig, l));
  return aiger_is_latch(const_cast<aiger *>(aig), l)->reset;
}
unsigned next(const aiger *aig, unsigned l) {
  assert(is_latch(aig, l));
  return aiger_is_latch(const_cast<aiger *>(aig), l)->next;
}
unsigned output(const aiger *aig) {
  if (aig->num_outputs)
    return aig->outputs[0].lit;
  else
    return aiger_false;
}

unsigned size(const aiger *aig) { return (aig->maxvar + 1) * 2; }

unsigned input(aiger *aig) {
  const unsigned new_var{size(aig)};
  aiger_add_input(aig, new_var, nullptr);
  assert(size(aig) != new_var);
  return new_var;
};

unsigned conj(aiger *aig, unsigned x, unsigned y) {
  const unsigned new_var{size(aig)};
  aiger_add_and(aig, new_var, x, y);
  return new_var;
};

unsigned eq(aiger *aig, unsigned x, unsigned y) {
  return aiger_not(conj(aig, aiger_not(conj(aig, x, y)),
                        aiger_not(conj(aig, aiger_not(x), aiger_not(y)))));
}

// consumes v
unsigned conj(aiger *aig, std::vector<unsigned> &v) {
  if (v.empty()) return 1;
  const auto begin = v.begin();
  auto end = v.cend();
  bool odd = (begin - end) % 2;
  end -= odd;
  while (end - begin > 1) {
    auto j = begin, i = j;
    while (i != end)
      *j++ = conj(aig, *i++, *i++);
    if (odd) *j++ = *end;
    odd = (begin - j) % 2;
    end = j - odd;
  }
  const unsigned res = *begin;
  v.clear();
  return res;
}

std::span<aiger_symbol> inputs(const aiger *aig) {
  return {aig->inputs, aig->num_inputs};
}
std::span<aiger_symbol> latches(const aiger *aig) {
  return {aig->latches, aig->num_latches};
}
std::span<aiger_and> ands(const aiger *aig) {
  return {aig->ands, aig->num_ands};
}

auto lits = std::views::transform([](const auto &l) { return l.lit; });
auto nexts = std::views::transform(
    [](const auto &l) { return std::pair{l.lit, l.next}; });
auto resets = std::views::transform(
    [](const auto &l) { return std::pair{l.lit, l.reset}; });
auto initialized =
    std::views::filter([](const auto &l) { return l.reset != l.lit; });
auto uninitialized =
    std::views::filter([](const auto &l) { return l.reset == l.lit; });

// Read only circuit for model and witness.
struct InAIG {
  aiger *aig;
  InAIG(const char *path) : aig(aiger_init()) {
    const char *err = aiger_open_and_read_from_file(aig, path);
    if (err) {
      std::cerr << "certifaiger: parse error reading " << path << ": " << err
                << "\n";
      exit(1);
    }
  }
  ~InAIG() { aiger_reset(aig); }
  aiger *operator*() const { return aig; }
};

// Wrapper for combinatorial circuits meant to be checked for validity via SAT.
struct OutAIG {
  aiger *aig; // combinatorial
  const char *path;
  OutAIG(const char *path) : aig(aiger_init()), path(path) {}
  ~OutAIG() {
    assert(!aig->num_latches);
    aiger_open_and_write_to_file(aig, path);
    aiger_reset(aig);
  }
  aiger *operator*() const { return aig; }
};
