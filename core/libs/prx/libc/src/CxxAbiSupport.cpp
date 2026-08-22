#include <exception>
#include <stdexcept>
#include <typeinfo>
#include <ios>
#include <system_error>
#include <locale>
#include <sstream>

namespace {

struct PolyBase { virtual ~PolyBase() = default; };
struct PolyDerived : PolyBase {};

volatile int g_sink = 0;

}

extern "C" void __anyps5_libc_touch_cxx_abi(int selector) {
    switch (selector) {
        case 0: { std::exception e; g_sink += e.what() != nullptr; break; }
        case 1: {
            try { throw std::runtime_error("x"); }
            catch (const std::exception& e) { g_sink += e.what() != nullptr; }
            break;
        }
        case 2: {
            try { throw std::out_of_range("x"); } catch (const std::exception&) { g_sink += 1; }
            break;
        }
        case 3: {
            try { throw std::domain_error("x"); } catch (const std::exception&) { g_sink += 1; }
            break;
        }
        case 4: {
            try { throw std::invalid_argument("x"); } catch (const std::exception&) { g_sink += 1; }
            break;
        }
        case 5: {
            try { throw std::logic_error("x"); } catch (const std::exception&) { g_sink += 1; }
            break;
        }
        case 6: {
            try { throw std::bad_cast(); } catch (const std::exception&) { g_sink += 1; }
            break;
        }
        case 7: {
            try { throw std::ios_base::failure("x"); } catch (const std::exception&) { g_sink += 1; }
            break;
        }
        case 8: {
            try {
                throw std::system_error(std::make_error_code(std::errc::invalid_argument));
            } catch (const std::exception&) { g_sink += 1; }
            break;
        }
        case 9: {
            PolyBase base;
            PolyBase* p = &base;
            PolyDerived* d = dynamic_cast<PolyDerived*>(p);
            g_sink += d == nullptr ? 1 : 0;
            break;
        }
        case 10: {
            std::locale loc = std::locale::classic();
            std::ostringstream oss;
            oss.imbue(loc);
            oss << 3;
            std::wostringstream woss;
            woss.imbue(loc);
            woss << L"a";
            std::use_facet<std::ctype<char>>(loc);
            std::use_facet<std::ctype<wchar_t>>(loc);
            std::use_facet<std::collate<wchar_t>>(loc);
            std::use_facet<std::num_put<char>>(loc);
            std::iostream_category();
            g_sink += static_cast<int>(oss.str().size());
            break;
        }
        default: break;
    }
}
