#ifndef LOGGER_HPP
#define LOGGER_HPP

/**
 * @file
 * @brief Provides Logger and macros around Logger::report*()
 *
 * The macros #repRaw, #repMsg, and #repErr try to use Logger::globalLogger
 * to report a message / error. If not possible, they just print the message.
 *
 * @author Jan-Lukas Wynen
 * @date 2016-12-07
 */

#include "def.hpp"

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
 * stream, written into a log file, or both.
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
    enum class Status : unsigned char {OK, NO_LOG_FILE, OP_FAILED, INVALID_USE};

    /// Default constructor
    /**
     * Does not set a log file and uses default parameters.
     */
    Logger();

    /// Assigns a log file
    /**
     * Other parameters are set to default values.
     * @param fname Name of the log file
     * @param app Append messages to existing log file or replace old file?
     */
    Logger(const char * const fname, const bool app = true);

    /**
     * Flushes the message queue, see flush()
     */
    ~Logger();

    
    /// Construct the global %Logger with default ctor
    /**
     * Stores the "global Logger" internally. If it already existed, the old
     * instance is deleted. *Not* thread-safe!
     * @return Pointer to the global %Logger
     */
    static Logger *buildGlobalLogger() noexcept;

    /// Construct the global %Logger with parameters
    /**
     * Stores the "global Logger" internally. If it already existed, the old
     * instance is deleted. The parameters are passed on to
     * Logger(const char * const, const bool). *Not* thread-safe!
     *
     * @param fname Name of the log file
     * @param app Append messages to existing log file or replace old file?
     * @return Pointer to the global %Logger
     */
    static Logger *buildGlobalLogger(const char * const fname, const bool app = true) noexcept;

    /// Removes global %Logger
    /**
     * Causes the message queue to be flushed.
     * @return Status::INVALID_USE if no globale %Logger was set, Status::OK else
     */
    static Status deleteGlobalLogger() noexcept;

    /// Gives access to global %Logger
    /**
     * Do <b><i>not</i></b> delete this %Logger instance manually, use deleteGlobalLogger() instead.
     * @return Pointer to global %Logger, nullptr if it has not been constructed.
     */
    static Logger *getGlobalLogger() noexcept;

    
    /// Set a (new) log file
    /**
     * Calls flush() before setting the file name if a log file was already specified.
     * Also resets firstWrite to true, see prepareLogFile.
     *
     * @param fname Name of the log file
     * @param app Append messages to existing log file or replace old file?
     * @return Status of flush() if called, Status::OK otherwise.
     */
    Status setLogFile(const char * const fname, const bool app = true) noexcept;

    /// Get the name of the log file
    /**
     * Copies the file name into a new c string.
     */
    char *getLogFile() noexcept;

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
    Status prepareLogFile(const char * const logName = nullptr,
                          const bool skipLock = false) noexcept;

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
    Status flush(const bool skipLock = false) noexcept;


    /// Show and log a string
    /**
     * Writes a string to a given stream and to the log file if one has been specified.
     * Uses showRaw() and logRaw().
     *
     * @param message String to be written
     * @param stream ID for stream to use, can be STDOUT_FILENO or STDERR_FILENO
     * @return Return value of showRaw() or logRaw() if former was Status::OK
     */
    Logger::Status reportRaw(const char * const message,
                             const int stream = STDOUT_FILENO) noexcept;

    /// Print a message to given stream
    /**
     * Writes to cout if stream==STDOUT_FILENO or cerr if stream==STDERR_FILENO
     * @param message String to be written
     * @param stream ID for stream to use, can be STDOUT_FILENO or STDERR_FILENO
     * @return Status::INVALID_USE if stream parameter not known, Status::OK otherwise
     */
    Logger::Status showRaw(const char * const message,
                           const int stream = STDOUT_FILENO) noexcept;

    /// Store a message in log file
    /**
     * Appends a string to the message queue and flushes it if it is longer than
     * maxQueueLength. Also stores the message if no file is specified.
     * @param message String to be written
     * @return Result of flush() if called, Status::OK otherwise
     */
    Logger::Status logRaw(const char * const message) noexcept;

    
    /// Show and log a formatted message
    /**
     * Writes a message to cout and to the log file if specified by using showMessage()
     * and logMessage().
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     * @return Result of logMessage() if log file is set
     */
    Logger::Status reportMessage(const char * const file, const unsigned int line,
                                 const char * const function, const char * const message) noexcept;

    /// Print a formatted message to cout
    /**
     * Writes a message formatted using shellColourCode() to cout.
     * See composeMessage() for details (uses error=false, insertTime=false, colour=true).
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     */
    void showMessage(const char * const file, const unsigned int line,
                     const char * const function, const char * const message) noexcept;
    
    /// Store a formatted message in log file
    /**
     * Appends a formatted message to the message queue.
     * It is flushed if it exceeds the maximum length.
     * Also stores the message if no file is specified.
     * See composeMessage() for details (uses error=false, insertTime=true, colour=false).
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     * @return Result of flush() if called
     */
    Logger::Status logMessage(const char * const file, const unsigned int line,
                              const char * const function, const char * const message) noexcept;

    
    /// Show and log a formatted error
    /**
     * Writes a message to cerr and to the log file if specified by using showError()
     * and logError().
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     * @return Result of logError() if log file is set
     */
    Logger::Status reportError(const char * const file, const unsigned int line,
                                 const char * const function, const char * const message) noexcept;

    /// Print a formatted error to cerr
    /**
     * Writes a message formatted using shellColourCode() to cerr.
     * See composeMessage() for details (uses error=true, insertTime=false, colour=true).
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     */
    void showError(const char * const file, const unsigned int line,
                     const char * const function, const char * const message) noexcept;

    /// Store a formatted error in log file
    /**
     * Appends a formatted error to the message queue.
     * It is flushed if it exceeds the maximum length.
     * Also stores the error message if no file is specified.
     * See composeMessage() for details (uses error=true, insertTime=true, colour=false).
     *
     * @param file Name of the file this function is called from
     * @param line Line-number this function is called from
     * @param function Name of the function this function is called from
     * @param message String to be written
     * @return Result of flush() if called
     */
    Logger::Status logError(const char * const file, const unsigned int line,
                              const char * const function, const char * const message) noexcept;


    /// Select whether using colours
    /**
     * Set whether showMessage() and showError() should use colours for their output.
     * Default is true.
     * @param clrd Use coloured output?
     */
    void setColoured(const bool clrd) noexcept;

    /// Set maximum length of message queue
    /**
     * Does not cause the queue to be flushed, this happens later when other, flushing
     * functions are called.
     * The default length is 5.
     * @param len New length for message queue
     */
    void setMaxQueueLength(const unsigned char len) noexcept;

    /// Get current maximum queue length
    /**
     * @return The maximum length of the message queue
     */
    unsigned char getMaxQueueLength() const noexcept;

    /// Make a name for log files
    /**
     * Construct a file name for log files of the form
     * "<name>_<date-time>.log". The date-time part is determined from the time
     * when this function is called. The underscore is omitted if name==nullptr.
     * 
     * @param name Optional name to prefix the file name
     * @return The constructed file name
     */
    static std::string makeLogName(const char * const name = nullptr) noexcept;
    
private:
    static Logger *globalLogger;  ///< The "global Logger" instance

    std::queue<std::string> messages;  ///< Message queue

    std::mutex lock;  ///< Mutex to lock down the entire object

    char *logFileName; ///< Name of log file. If nullptr, no file is used
    
    bool append;       ///< Append new content to old file?
    bool coloured;     ///< Use coloured output?
    bool firstWrite;   ///< Is there already a header in the file?
    
    unsigned char maxQueueLength;  ///< Maximum length of messages queue

    
    /// Construct a string from current time
    /**
     * Build a string from the current time in the format
     * <TT>"YYYY-MM-DDThh-mm-ss"</TT> (M=month, m=minute).
     * @return The time string
     */
    static char *makeTimeString() noexcept;

    
    /// Combine inputs into message string
    /**
     * General function to create a formatted message.
     * The format is
     * <TT>"(\<date-time\>) ERROR [\<file\> | \<line\> | \<function\>] message"</TT>.
     * - <TT>\<date-time\></TT> is built using makeTimeString(), it is omitted if insertTime==false
     *   (including the parentheses)
     * - ERROR is only present when error==true
     * - <TT>\<file\></TT> and <TT>\<line\></TT> are omitted when file==nullptr
     * - <TT>\<function\></TT> is omitted if function==nullptr
     * The output contains colour codes obtained from shellColourCode() of colour==true
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
    std::string composeMessage(const char * const file, const unsigned int line,
                               const char * const function, const char * const message,
                               const bool error, const bool insertTime, const bool colour)
        const noexcept;
};

#ifndef repRaw
/// Convenienece wrapper around Logger::reportRaw()
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
        cout << msg << endl;                                        \
    }
#else
  #error "repRaw is already defined"
#endif

#ifndef repMsg
/// Convenienece wrapper around Logger::reportMessage()
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
        cout << "[" << __FILE__ << " | " << __LINE__ << " | "       \
             << __func__ << "]: "                                   \
             << msg << endl;                                        \
    }
#else
  #error "repMsg is already defined"
#endif

#ifndef repErr
/// Convenienece wrapper around Logger::reportError()
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
        cerr << "ERROR [" << __FILE__ << " | " << __LINE__ << " | " \
             << __func__ << "]: "                                   \
             << msg << endl;                                        \
    }
#else
  #error "repErr is already defined"
#endif


class MegaLogger : public(Logger) {
    
};


#endif  // ndef LOGGER_HPP
