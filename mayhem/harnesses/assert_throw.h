// Force-included into every compiled OpenSpace TU by mayhem/build.sh.
//
// The build defines OPENSPACE_THROW_ON_ASSERT (upstream's own switch) so a failed
// assert_msg throws openspace::AssertionException instead of prompting on stdin (which
// would hang a fuzzer). The macro names the exception unqualified, which only resolves
// inside namespace openspace; this using-declaration makes it resolve everywhere.
#pragma once
#include <openspace/misc/assert.h>
using openspace::AssertionException;
