// Compiles against the installed headers only (no source tree on the include path).
#include "se050_device.hpp"
#include "se050_version.h"

static_assert(HF_SE050_VERSION_MAJOR == 0, "installed version header");

int main() {
  se050::scp03::ChannelState st{};
  se050::scp03::WipeChannel(st);
  return st.open ? 1 : 0;
}
