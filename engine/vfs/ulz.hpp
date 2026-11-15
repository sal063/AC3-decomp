#pragma once

#include "core/bytes.hpp"

namespace ac3::vfs::ulz {
enum class Error { none, not_ulz, bad_type, bad_window, truncated, bad_reference, overrun };

const char* error_name(Error e);

bool is_ulz(ByteView in);
u32 decoded_size(ByteView in);
Error decode(ByteView in, Bytes& out);
}
