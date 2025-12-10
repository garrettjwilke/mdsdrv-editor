// Embedded mdsdrv.bin implementation.
#include "mdsdrv_bin.h"

// xxd-generated payload; keep in an anonymous namespace to avoid symbol leaks.
namespace {
#include "mdsdrv_bin.inc"
} // namespace

// Public pointers to the embedded data/size.
const unsigned char* g_mdsdrv_bin = mdsdrv_bin;
const std::size_t g_mdsdrv_bin_size = mdsdrv_bin_len;
