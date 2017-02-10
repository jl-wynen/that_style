#ifndef UTIL_STREAMWRAPPER_HPP
#define UTIL_STREAMWRAPPER_HPP

#include <limits>
#include <istream>
#include <string>
#include <type_traits>

/** @file
 * @brief Provides StreamWrapper and related functions
 * @author Jan-Lukas Wynen
 */


/// Determines whether type <TT>T</TT> allows for NaN or Inf
/**
 * Checks whether StreamWrapper can recover from a failed attempt to input a value
 * of type <TT>T</TT>. Recovering is possible iff <TT>T</TT>
 * - is floating point,
 * - can represent a quiet NaN, and
 * - can represent infinity.
 */
template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type...>
constexpr inline bool input_is_recoverable() {
    return std::numeric_limits<T>::has_quiet_NaN
        && std::numeric_limits<T>::has_infinity;
}

// extra overload to make this work for stream manipulators (functions)
template <typename T, typename std::enable_if<!std::is_floating_point<T>::value>::type...>
constexpr inline bool input_is_recoverable() {
    return false;
}


// expects ASCII encoding
// provides RAII access to a child of std::istream

/// Wrapper around std::istream to parse <TT>NaN</TT> and <TT>Inf</TT>
/**
 * flag value | indicates
 * ----------:|:----------
 * goodbit    | Everything if ok / absence of any flags
 * badbit     | Operation on the stream buffer failed
 * failbit    | Input failed because of the internal logic of the stream (parsing)
 * eofbit     | End-of-file was reached during an operation
 * nanbit     | <TT>NaN</TT> was extracted
 * infbit     | <TT>Inf</TT> was extracted
 */
class StreamWrapper {
public:

    using iostate = uint_least8_t; ///< Type to store current state of stream
    
    static const iostate goodbit = 0;      ///< Good-bit for error state
    static const iostate badbit  = 1 << 0; ///< Bad-bit for error state
    static const iostate eofbit  = 1 << 1; ///< End-of-file-bit for error state
    static const iostate failbit = 1 << 2; ///< Fail-bit for error state
    static const iostate nanbit  = 1 << 3; ///< Not-a-number-bit for error state
    static const iostate infbit  = 1 << 4; ///< Infinity-bit for error state


    /// Creates a new StreamWrapper instance
    /**
     * <TT>T</TT> shall be <TT>std::istream</TT> or derived from it.
     * Also creates a new instance of <TT>T</TT> and assigns it to the
     * new %StreamWrapper.
     *
     * @param args Argument pack to be passed to the constructor of the <TT>istream</TT>.
     * @return The new %StreamWrapper instance
     */
    template <typename T, typename... Args>
    friend StreamWrapper makeStreamWrapper(Args... args);

    /// Creates a new StreamWrapper instance on the heap
    /**
     * <TT>T</TT> shall be <TT>std::istream</TT> or derived from it.
     * Also creates a new instance of <TT>T</TT> and assigns it to the
     * new %StreamWrapper.
     *
     * @param args Argument pack to be passed to the constructor of the <TT>istream</TT>.
     * @return Pointer to the new %StreamWrapper instance
     */    
    template <typename T, typename... Args>
    friend StreamWrapper *makeStreamWrapperD(Args... args);
    

    /// Deleted copy constructor
    /**
     * Copying is not allowed because it would invalidate one instance and/or
     * create a second pointer to StreamWrapper::stream.
     */
    StreamWrapper(StreamWrapper const&) = delete;

    /// Move constructor
    /**
     * Makes a shallow copy and invalidates `other.stream` to ensure correct destruction.
     */
    StreamWrapper(StreamWrapper &&other);

    /// Destructor
    /**
     * Deletes `this->stream` which closes the stream. I.e. in case `this->stream` is an
     * <TT>std::ifstream</TT> the file is closed.
     */
    ~StreamWrapper();

    
    /// Assign a new std::ctype to StreamWrapper::stream
    /**
     * Modifies the locale of `this->stream` to use the provided <TT>ctype</TT>.
     * @param ct std::ctype instance to be used from now on. The stream assumes control
     *   over the memory of cv. Do not use it elsewhere!
     * @return The locale previously associated with `this->stream`
     * @see SpaceSelectorCT for a ctype implementation that treats arbitrary
     *   characters as spaces
     */
    std::locale setCType(std::ctype<char> *ct);


    /// Wrapper around <TT>std::ios::good()</TT>
    bool good() const;
    
    /// Wrapper around <TT>std::ios::bad()</TT>
    bool bad() const;

    /// Wrapper around <TT>std::ios::eof()</TT>
    bool eof() const;

    /// Wrapper around <TT>std::ios::fail()</TT>
    bool fail() const;

    /// Returns <TT>true</TT> iff nanbit or infbit are set in `this->errorState`.
    bool readNaN() const;
    
    /// Returns <TT>true</TT> iff infbit are set in `this->errorState`.
    bool readInf() const;

    
    /// Clears the error state
    /**
     * Behaves like <TT>std::ios_base::clear()</TT>.
     * Overwrites the error states of the %StreamWrapper and its istream.
     * @param state New error state to overwrite old one. Should be a bitwise
     *   combination of the %StreamWrapper error bits not those form the standard library.
     */
    void clear(StreamWrapper::iostate state = StreamWrapper::goodbit);
    
    /// Reads the error state
    /**
     * Behaves like <TT>std::ios_base::rdstate()</TT>.
     * All error bits from the istream are translated to the bits
     * provided by %StreamWrapper.
     * @return The error state of the %StreamWrapper and its istream. Test agains the
     *   bits provided by %StreamWrapper not those from the standard library.
     */
    StreamWrapper::iostate rdstate() const;
    
    /// Adds new bits to the error state
    /**
     * Behaves like <TT>std::ios_base::setstate()</TT>.
     * Only adds bits to the error state variable without removing already set ones.
     * @param state Combination of new bits to set. Use the bits provided by %StreamWrapper
     *   not those from the standard library.
     */
    void setstate(StreamWrapper::iostate state);


    /// Applies a stream manipulator to StreamWrapper::stream
    /**
     * Overload of the input operator for stream manipulators.
     * @param pf Manipulator to apply
     * @return `*this`
     */
    StreamWrapper &operator>>(std::ios_base &(*pf)(std::ios_base&)) {
        *this->stream >> pf;
        return *this;
    }
    
    /// Extracts a field from StreamWrapper::stream
    /**
     * This overload is called if type <TT>T</TT> does not allow for NaN and Inf values, see
     * input_is_recoverable(). Requires <TT>operator>>(istream, T)</TT> to be defined.<BR>
     * Performs a normal input operation and does not set nanbit of infbit.
     *
     * @param x Variable that the extraced field is stored into
     * @return `*this`
     */
    template <typename T, typename std::enable_if<!input_is_recoverable<T>()>::type...>
    StreamWrapper &operator>>(T &x) {
        *this->stream >> x;
        return *this;
    }

    /// Extracts a field from StreamWrapper::stream
    /**
     * This overload is called if type <TT>T</TT> allows for NaN and Inf values, see
     * input_is_recoverable(). Requires <TT>operator>>(istream, T)</TT> to be defined.<BR>
     * If input fails, a recovery is attempted trying to read <TT>NaN</TT> or
     * <TT>Inf</TT> from the stream (capitalizaion does not matter).
     * If successful, either nanbit or infbit are set. Otherwise, failbit is set and
     * <TT>x</TT> is undefined.
     *
     * @param x Variable that the extraced field is stored into
     * @return `*this`
     */
    template <typename T, typename std::enable_if<input_is_recoverable<T>()>::type...>
    StreamWrapper &operator>>(T &x) {
        // detect sign first (cannot be recovered easily after stream >> x)
        bool neg = false;
        while (std::isspace(this->stream->peek())) // skip all spaces
            this->stream->ignore();
        if (this->stream->peek() == '-')
            neg = true;

        // try a normal extraction
        *this->stream >> x;

        // something went wrong -> attempt to recover
        if (this->stream->fail()) {
            if (this->stream->bad())   // real error
                return *this;

            this->stream->clear();

            // extract string
            std::string str;
            *this->stream >> str;

            if (this->stream->fail())
                return *this;

            // not a candidate for NaN or Inf
            if (str.length() != 3) {
                this->setstate(StreamWrapper::failbit);
                return *this;
            }
            
            if ((str[0] == 'n' || str[0] == 'N')
                && (str[1] == 'a' || str[1] == 'A')
                && (str[2] == 'n' || str[2] == 'N')) {

                if (neg)
                    x = -std::numeric_limits<T>::quiet_NaN();
                else
                    x = std::numeric_limits<T>::quiet_NaN();
                this->setstate(StreamWrapper::nanbit);
            }
            else if ((str[0] == 'i' || str[0] == 'I')
                && (str[1] == 'n' || str[1] == 'N')
                && (str[2] == 'f' || str[2] == 'F')) {

                if (neg)
                    x = -std::numeric_limits<T>::infinity();
                else
                    x = std::numeric_limits<T>::infinity();
                this->setstate(StreamWrapper::infbit);
            }
            else {
                this->setstate(StreamWrapper::failbit);
            }
        }
        
        return *this;
    }

private:
    std::istream *stream;  ///< Stream to use for the actual input operations. Is managed entirely by the %StreamWrapper.

    iostate errorState;  ///< Combination of nanbit and infbit, all other erro bits are set in StreamWrapper::stream only.

    /// Constructs a new instance and assigns an `std::istream`
    /**
     * This constructor is private to ensure that the stream is not available
     * elsewhere. Use makeStreamWrapper() and makeStreamWrapperD to
     * instanciate %StreamWrapper.
     *
     * @param st <TT>std::istream</TT> to be used by the %StreamWrapper.
     */
    StreamWrapper(std::istream * const st);
};


template <typename T, typename... Args>
StreamWrapper makeStreamWrapper(Args... args) {
    static_assert(std::is_base_of<std::istream,T>(), "makeStreamWrapper: Type T is not derived from std:istream");
    return StreamWrapper(new T(args...));
}

template <typename T, typename... Args>
StreamWrapper *makeStreamWrapperD(Args... args) {
    static_assert(std::is_base_of<std::istream,T>(), "makeStreamWrapper: Type T is not derived from std:istream");
    return new StreamWrapper(new T(args...));
}


#endif
