// symbol_helper.hpp: helper macros that define the boiler plate for exporting functions
#define DYNALO_EXPORT_SYMBOLS
#include <dynalo/symbol_helper.hpp>

#include <cstdint>

DYNALO_EXPORT int32_t DYNALO_CALL add_integers(const int32_t a, const int32_t b);
DYNALO_EXPORT void DYNALO_CALL print_message(const char* message);
