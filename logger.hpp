#ifndef LOGGER_HPP
#define LOGGER_HPP

/**
 * @file
 * @brief Provides Logger and macros around Logger::report*()
 *
 * The macros #repRaw, #repMsg, and #repErr try to use Logger::globalLogger
 * to report a message / error. If not possible, they just print the message to cout
 * and cerr respectively.
 *
 * @author Jan-Lukas Wynen
 */

#include "util_def.hpp"

#include <queue>
#include <string>
#include <mutex>
#include <iostream>

#if IS_UNIX
#include <unistd.h>
#endif



/// Class to print output to terminal and file
/**
 * This class systematically handles text messages. Messages can be printed to a standard
 * stream, written into a log file, or both. \cite Hale
 *
 * Three types of messages are supported:
 * - Simple, or "raw", strings are printed unchanged
 * - Messages are formatted to give extra information about their origin
 * - Errors are formatted like messages with the extra flag "ERROR" and written to cerr
 *
 * Each type of message can be printed using one of three functions:
 * - <TT>show*()</TT> just prints the (formatted) message to cout or cerr
 * - <TT>log*()</TT> writes the message to a log file
 * - <TT>report*()</TT> does both of the above
 *
 * All operations are possible when no file is specified but they do not print to any file
 * and return Logger::Status::NO_LOG_FILE in this case.
 * If one is set, messages are not written to the file immediately but added to a queue.
 * This queue is flushed when it exceeds a configurable maximum length or when the %Logger
 * instance is destroyed.
 *
 * A static member provides access to a "global Logger" which can be managed through the
 * corresponding functions. It can be set up once through Logger::buildGlobalLogger() and
 * used throughout the program. In order to flush the message queue to file,
 * Logger::deleteGlobalLogger should be called at the end of the program. Do *not* delete
 * the global %Logger manually as this will corrupt the function of the functions that
 * manage it.
 * 
 * All functions are thread safe except for the static member functions. In particular the
 * global %Logger should not be build/deleted in a parallel environment.
 * Logger thus provides a safe way to handle output in a multi-threaded program.
 */
class Logger {
public:
    /// Return value for many member functions
    /**
     * Encodes the success or failure of operations done by %Logger.
     */
    enum class Status : uint_least8_t {OK, NO_LOG_FILE, OP_FAILED, INVALID_USE};

    using flag_t = unsigned short;   ///< Type for flags in Properties

    static flag_t const coloured       = 1 << 0;  ///< Bit for coloured output (See Logger::Properties)
    static flag_t const logDate        = 1 << 1;  ///< Bit to request logging the date (See Logger::Properties)
    static flag_t const logTime        = 1 << 2;  ///< Bit to request logging the time (See Logger::Properties)
    static flag_t const breakLinesTTY  = 1 << 3;  ///< Bit to indicate that lines shall be broken f too long for show* functions (See Logger::Properties)
    static flag_t const breakLinesFile = 1 << 4;  ///< Bit to indicate that lines shall be broken if too long for log* functions (See Logger::Properties)
    static flag_t const doExtraIndent  = 1 << 5;  ///< Bit to toggle extra indentation for alignment (See Logger::Properties)

    /// POD collection of properties to control output of Logger
    struct Properties {
        flag_t flags;  ///< Bitwise combination of Logger::coloured, Logger::logDate, Logger::logTime, Logger::breakLinesTTY, Logger::breakLinesFile, and Logger::doExtraIndent
        unsigned short indent;  ///< Depth of indentation of each line
        unsigned short maxLineLengthTTY;  ///< Maximum length of lines on a terminal. Automatically determined if set to 0.
        unsigned short maxLineLengthFile; ///< Maximum length of lines on a terminal. Uses value of maxLineLengthTTY if set to 0.
        /// Lets go really deep
        class PropsOfProps {
            PropsOfProps() = default;
            explicit PropsOfProps(const Logger &l) {}
            PropsOfProps(PropsOfProps &other) = delete;
        };
    };

    
    /// Construct without assigning a log file
    /**
     * @param p Output properties to use for this instance. Uses defaults if not specified.
     * @see Logger::getDefaultProperties()
     */
    Logger(Properties const &p = Logger::getDefaultProperties());

    /// Assigns a log file
    /**
     * @param fname Name of the log file
     * @param app Append messages to existing log file or replace old file?
     * @param p Output properties to use for this instance. Uses defaults if not specified.
     * @see Logger::getDefaultProperties()
     */
    Logger(std::string const &fname, bool const app = true,
           Properties const &p = Logger::getDefaultProperties());

    /// Destructor
    /**
     * Flushes the message queue, see flush()
     */
    ~Logger();

    
    /// Construct the global %Logger without assigning a file
    /**
     * Stores the "global Logger" internally. If it already existed, the old
     * instance is deleted.
     * @note Not thread safe
     *
     * @warning watch out
     * @attention look here
     * @pre before exec
     * @post after exec
     * @invariant Stays the same
     * @deprecated Don't use anymore
     * @todo TODO
     * @test Testing
     * @bug A bug
     *
     *
     * @param p p Output properties passed to the constructor
     * @return Pointer to the global %Logger
     */
    static Logger *buildGlobalLogger(Properties const &p = Logger::getDefaultProperties()) noexcept;

    /// Construct the global %Logger and assigns a file
    /**
     * Stores the "global Logger" internally. If it already existed, the old
     * instance is deleted. The parameters are passed on to
     * Logger(const char * const, const bool).
     * @note Not thread safe
     *
     * @param fname Name of the log file
     * @param app Append messages to existing log file or replace old file?
     * @param p p Output properties passed to the constructor
     * @return Pointer to the global %Logger
     */
    static Logger *buildGlobalLogger(std::string const &fname, bool const app = true,
                                     Properties const &p = Logger::getDefaultProperties()) noexcept;

    /// Removes global %Logger
    /**
     * Causes the message queue to be flushed.
     * @return Status::INVALID_USE if no global %Logger was set, Status::OK else
     */
    static Status deleteGlobalLogger() noexcept;

    /// Gives access to global %Logger
    /**
     * Do <b><i>not</i></b> delete this %Logger instance manually, use deleteGlobalLogger() instead.
     * @return Pointer to global %Logger, nullptr if it has not been constructed.
     */
    static Logger *getGlobalLogger() noexcept;

    

    Status setLogFile(std::string const &fname, bool const app = true);

    /// Get the name of the log file
    /**
     * Copies the file name into a new string.
     */
    std::string getLogFile();

    /// Insert a header into the log file
    /**
     * The header can be used to signal the start of a new log in a file
     * (e.g. new run of the program). It shows an optional name and date and time of
     * the call to this function.<BR>
     * Is called by flush() without a name when it first writes to a file
     * or setLogFile() has been called before.
     *
     * @param logName Name that shows up in the header
     * @param skipLock Do <b><i>not</i></b> set unless you know, what you are doing!
     * @return
     *   - Status::NO_LOG_FILE if no file has been specified
     *   - Status::OP_FAILED if the file operations failed, see ouput to cerr
     *   - Status::OK otherwise
     */
    // Does not regard mutex if skipLock = true
    Status prepareLogFile(std::string const &logName = std::string(),
                          bool const skipLock = false);

    /// Flushes message queue to file
    /**
     * Writes the message queue into the assigned file. Also calls prepareLogFile() without
     * a name if it writes for the first time or setLogFile() has been called.
     *
     * @param skipLock Do <b><i>not</i></b> set unless you know, what you are doing!
     * @return
     *   - Status::NO_LOG_FILE if no file has been specified
     *   - Status::OP_FAILED if the file operations failed, see ouput to cerr
     *   - Status::OK otherwise
     */
    // Does not regard mutex if skipLock = true
    Status flush(bool const skipLock = false);


    /// Show and log a string without formatting
    /**
     * Writes a string to a given stream and to the log file if one has been specified.
     * Uses showRaw() and logRaw().
     *
     * @param message String to be written
     * @param stream ID for stream to use, can be STDOUT_FILENO or STDERR_FILENO
     * @return Logger::Status::NO_LOG_FILE if no log file is specified.
     *     Otherwise return value of showRaw() or logRaw() if former was Status::OK
     */
    Logger::Status reportRaw(std::string const &message,
                             int const stream = STDOUT_FILENO);

    /// Print a message to given stream without formatting
    /**
     * Writes to cout if stream==STDOUT_FILENO or cerr if stream==STDERR_FILENO
     * @param message String to be written
     * @param stream ID for stream to use, can be STDOUT_FILENO or STDERR_FILENO
     * @return Status::INVALID_USE if stream parameter not known, Status::OK otherwise
     */
    Logger::Status showRaw(std::string const &message,
                           int const stream = STDOUT_FILENO);

    /// Store a message in log file without formatting
    /**
     * Appends a string to the message queue and flushes it if it is longer than
     * maxQueueLength. Also stores the message if no file is specified.
     * @param message String to be written
     * @return Result of flush() if called, Status::OK otherwise
     */
    Logger::Status logRaw(std::string const &message);

    
    /// Show and log a formatted message
    /**
     * Writes a message to cout and to the log file if specified by using showMessage()
     * and logMessage().\n
     * Uses Properties instance of Logger to control formatting. Can be overwritten with
     * parameter props.
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     * @param props Properties to control formatting of the message. If nullptr, uses outputProps.
     * @return Logger::Status::NO_LOG_FILE if no log file is specified.
     *     Otherwise return logMessage().
     */
    Logger::Status reportMessage(std::string const &file, unsigned int const line,
                                 std::string const &function, std::string const &message,
                                 Properties const *props = nullptr);

    /// Print a formatted message to cout
    /**
     * Writes a message formatted using composeMessage() to cout.\n
     * Uses Properties instance of Logger to control formatting. Can be overwritten with
     * parameter props. Allows using colour but supresses date and time strings.
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     * @param props Properties to control formatting of the message. If nullptr, uses outputProps.
     */
    void showMessage(std::string const &file, unsigned int const line,
                     std::string const &function, std::string const &message,
                     Properties const *props = nullptr);
    
    /// Store a formatted message in log file
    /**
     * Appends a formatted message to the message queue which is flushed if it exceeds
     * the maximum length. Also stores the message if no file is specified.\n
     * The message if formatted using composeMessage().
     * Uses Properties instance of Logger to control formatting. Can be overwritten with
     * parameter props. Allows date and time strings but supresses colour.
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     * @param props Properties to control formatting of the message. If nullptr, uses outputProps.
     * @return Result of flush() if called
     */
    Logger::Status logMessage(std::string const &file, unsigned int const line,
                              std::string const &function, std::string const &message,
                              Properties const *props = nullptr);

    
    /// Show and log a formatted error
    /**
     * Writes a message to cerr and to the log file if specified by using showError()
     * and logError().
     * Uses Properties instance of Logger to control formatting. Can be overwritten with
     * parameter props.
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     * @param props Properties to control formatting of the message. If nullptr, uses outputProps.
     * @return Logger::Status::NO_LOG_FILE if no log file is specified.
     *     Otherwise return value of showError().
     */
    Logger::Status reportError(std::string const &file, unsigned int const line,
                               std::string const &function, std::string const &message,
                               Properties const *props = nullptr);

    /// Print a formatted error to cerr
    /**
     * Writes a message formatted using composeMessage() to cerr.\n
     * Uses Properties instance of Logger to control formatting. Can be overwritten with
     * parameter props. Allows using colour but supresses date and time strings.
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     * @param props Properties to control formatting of the message. If nullptr, uses outputProps.
     */
    void showError(std::string const &file, unsigned int const line,
                   std::string const &function, std::string const &message,
                   Properties const *props = nullptr);

    /// Store a formatted error in log file
    /**
     * Appends a formatted message to the message queue which is flushed if it exceeds
     * the maximum length. Also stores the message if no file is specified.\n
     * The message if formatted using composeMessage().
     * Uses Properties instance of Logger to control formatting. Can be overwritten with
     * parameter props. Allows date and time strings but supresses colour.
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     * @param props Properties to control formatting of the message. If nullptr, uses outputProps.
     * @return Result of flush() if called
     */
    Logger::Status logError(std::string const &file, unsigned int const line,
                            std::string const &function, std::string const &message,
                            Properties const *props = nullptr);

    /// Set maximum length of message queue
    /**
     * Does not cause the queue to be flushed, this happens later when other, flushing
     * functions are called.
     * The default length is 5.
     * @param len New length for message queue
     */
    void setMaxQueueLength(unsigned int const len) noexcept;

    /// Get current maximum queue length
    /**
     * @return The maximum length of the message queue
     */
    unsigned int getMaxQueueLength() const noexcept;

    /// Return default output properties
    /**
     * @return New instance of Logger::Properties initialized with default parameters.
     */
    static constexpr Properties getDefaultProperties() noexcept {
        return {Logger::coloured | Logger::logTime
                | Logger::breakLinesTTY | Logger::breakLinesFile | Logger::doExtraIndent,
                0, 0, 0};
    }    

    /// Return current instance of Properties
    /*
     * @return Copy of outputProps
     */
    Properties getProperties() noexcept;

    /// Set new output properties
    /**
     * Copies the parameter into outputProps.
     * @param props New output properties to use
     */
    void setProperties(Properties const &props);
    
    /// Make a name for log files
    /**
     * Construct a file name for log files of the form
     * "<name>_<date-time>.log". The date-time part is determined from the time
     * when this function is called. The underscore is omitted if name==nullptr.
     * 
     * @param name Optional name to prefix the file name
     * @return The constructed file name
     */
    static std::string makeLogName(std::string const &name = std::string());
    
private:
    static Logger *globalLogger;  ///< The "global Logger" instance

    std::queue<std::string> messages;  ///< Message queue

    std::mutex lock;  ///< Mutex to lock down the entire object

    std::string logFileName; ///< Name of log file. If empty, no file is used

    Properties outputProps;  ///< Controls message formatting

    unsigned int maxQueueLength;  ///< Maximum length of messages queue
    
    bool append;       ///< Append new content to old file?
    bool firstWrite;   ///< Is there already a header in the file?
    
    /// Combine inputs into message string
    /**
     * General function to create a formatted message.
     * The format is
     * <TT>"(<date-time>) ERROR [<file> | <line> | <function>] message"</TT>.
     * - <TT>\<date-time\></TT> is built using makeTimeString(), it is omitted if insertTime==false
     *   (including the parentheses)
     * - ERROR is only present when error==true
     * - <TT>\<file\></TT> and <TT>\<line\></TT> are omitted when file==nullptr
     * - <TT>\<function\></TT> is omitted if function==nullptr
     *
     * The output contains colour codes obtained from shellColourCode() if colour==true.
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message Arbitrary text
     * @param error Is this an error message?
     * @param insertTime Include time in the string?
     * @param colour Include colour strings?
     *
     * @return The constructed message
     */
    std::string composeMessage(std::string const &file, unsigned int const line,
                               std::string const &function, std::string const &message,
                               bool const error, bool const toFile,
                               Properties const * props = nullptr) const;
};

#ifndef repRaw
/// Convenience wrapper around Logger::reportRaw()
/**
 * Uses the global Logger to report a raw message.
 * Writes the message to cout if the global %Logger is not set.
 *
 * @param msg Message to report
 */
  #define repRaw(msg) {                                             \
    Logger *_logger_ = Logger::getGlobalLogger();                   \
    if (_logger_ != nullptr)                                        \
        _logger_->reportRaw(msg, STDOUT_FILENO);                    \
    else                                                            \
        std::cout << msg << std::endl;                              \
    }
#else
  #error "repRaw is already defined"
#endif

#ifndef repMsg
/// Convenience wrapper around Logger::reportMessage()
/**
 * Uses the global Logger to report a message and automatically supplies
 * the file, line, and function arguments.<BR>
 * Writes the message to cout if the global %Logger is not set.
 *
 * @param msg Message to report
 */
  #define repMsg(msg) {                                             \
    Logger *_logger_ = Logger::getGlobalLogger();                   \
    if (_logger_ != nullptr)                                        \
        _logger_->reportMessage(__FILE__, __LINE__, __func__, msg); \
    else                                                            \
        std::cout << "[" << __FILE__ << " | " << __LINE__ << " | "  \
                  << __func__ << "]: "                              \
                  << msg << std::endl;                              \
    }
#else
  #error "repMsg is already defined"
#endif

#ifndef repErr
/// Convenience wrapper around Logger::reportError()
/**
 * Uses the global Logger to report an error and automatically supplies
 * the file, line, and function arguments.<BR>
 * Writes the message to cout if the global %Logger is not set.
 *
 * @param msg Erro message to report
 */
  #define repErr(msg) {                                             \
    Logger *_logger_ = Logger::getGlobalLogger();                   \
    if (_logger_ != nullptr)                                        \
        _logger_->reportError(__FILE__, __LINE__, __func__, msg);   \
    else                                                            \
        std::cerr << "ERROR [" << __FILE__ << " | " << __LINE__     \
                  << " | " << __func__ << "]: "                     \
                  << msg << std::endl;                              \
    }
#else
  #error "repErr is already defined"
#endif

#endif  // ndef LOGGER_HPP
