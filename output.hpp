#ifndef OUTPUT_HPP
#define OUTPUT_HPP

/** @file
 * @brief A collection of useful output routines
 *
 * @author Jan-Lukas Wynen
 * @date 2016-12-02
 */

#include "def.hpp"

#include <cstdint>
#include <string>

#if IS_UNIX
#include <unistd.h>
#endif

/// Represent colours for text output
/**
 * The values are set to reflect the colour codes for terminals.
 */
enum class Colour : uint_least8_t {
    BLACK=0, RED=1, GREEN=2, YELLOW=3, BLUE=4, PURPLE=5, CYAN=6, WHITE=7, DEFAULT
};

/// Represent text properties for output to shell
class TextProperties {
public:
    /**
     * Default constructor. Initializes instance with default text parameters, can be
     * used to clear colour codes.
     */
    TextProperties() : foreground(Colour::DEFAULT), highIntensityFG(false),
                       background(Colour::DEFAULT), highIntensityBG(false),
                       modifier(NORMAL) {}

    /**
     * Initialize Fourground colour and set all other parameters to default.
     * @param fg Foreground colour
     * @param ifg Switch to use a high intensity foreground colour
     */
    TextProperties(const Colour fg, const bool ifg = false)
        : foreground(fg), highIntensityFG(ifg),
          background(Colour::DEFAULT), highIntensityBG(false),
          modifier(NORMAL) {}
    
    /**
     * Initializes all parameters with arguments.
     * @param fg Foreground colour
     * @param ifg Switch to use a high intensity foreground colour
     * @param bg Background colour
     * @param ibg Switch to use a high intensity background colour
     * @param mod Text modifier.
     */
    TextProperties(const Colour fg, const bool ifg,
                   const Colour bg, const bool ibg,
                   const uint_least8_t mod)
        : foreground(fg), highIntensityFG(ifg),
          background(bg), highIntensityBG(ibg),
          modifier(mod) {}

    Colour foreground;       ///< Foreground colour
    bool highIntensityFG;    ///< Switch for high intensity foreground colour
    Colour background;       ///< Background colour
    bool highIntensityBG;    ///< Switch for high intensity background colour
    uint_least8_t modifier;  ///< Modifiers for text
    
    static const uint_least8_t NORMAL     = 0x0;      ///< Default text modifier (plain text)
    static const uint_least8_t BOLD       = 0x1;      ///< Modifier for bold face
    static const uint_least8_t DIM        = 0x1 << 1; ///< Modifier for dim colour
    static const uint_least8_t SLANT      = 0x1 << 2; ///< Modifier for slanted face
    static const uint_least8_t UNDERLINE  = 0x1 << 3; ///< Modifier for underlined face
    static const uint_least8_t BLINK      = 0x1 << 4; ///< Modifier for blinking text
    static const uint_least8_t INVERSE    = 0x1 << 5; ///< Modifier to invert foreground and background
    static const uint_least8_t HIDDEN     = 0x1 << 6; ///< Modifier for hidden text
    static const uint_least8_t STRIKE_OUT = 0x1 << 7; ///< Modifier for striked out face
};

/// Create an ANSI/VT100 escape sequence
/**
 * @param tp Properties to encode
 * @return Escape sequence encoding \p tp
 */
const std::string shellColourCode(const TextProperties &tp) noexcept;

/// Create an ANSI/VT100 escape sequence if useful
/**
 * Only returns a colour code if <TT>IS_UNIX</TT> is true and the selected stream is
 * connected to a terminal.
 * @param tp Properties to encode
 * @param stream Stream to check whether colour codes can be applied. Can be
 *   <TT>STDOUT_FILENO</TT> or <TT>STDERR_FILENO</TT>.
 * @return Escape sequence encoding \p tp
 */
const std::string shellColourCode(const TextProperties &tp, const int stream) noexcept;

/// Print text with given properties
/** 
 * Use a TextProperties instance to create a terminal escape sequence and print a string,
 * appending a linefeed.
 * Colour codes are only used when appropriate, see shellColourCode(const TextProperties &tp, const int stream).
 *
 * @param str String to output
 * @param tp TextProperties instance to pass to shellColourCode()
 * @param stream Identifier for output stream to print to. Use <TT>STDOUT_FILENO</TT>
 *     or <TT>STDERR_FILENO</TT> cout and cerr, respectively.
 */
#if IS_UNIX
void printPropertized(const std::string &str, const TextProperties &tp,
                      const int stream = STDOUT_FILENO) noexcept;
#else
void printPropertized(const std::string &str, const TextProperties &tp,
                      const int stream = 0) noexcept;
#endif

#endif  // ndef OUTPUT_HPP
