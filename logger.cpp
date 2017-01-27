#include "logger.hpp"

#include <cstring>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <algorithm>

#include "output.hpp"

using namespace std;

// ----- init static member -----
Logger *Logger::globalLogger = nullptr;


// ----- ctors -----
Logger::Logger(Properties const &p)
    : logFileName(), outputProps(p), maxQueueLength(10), append(true), firstWrite(true) {
}


Logger::Logger(std::string const &fname, bool const app,
               Properties const &p)
    : logFileName(fname), outputProps(p),  maxQueueLength(10), append(app), firstWrite(true) {
}


// ----- dtor -----
Logger::~Logger() {
    this->flush();
}


// ----- manage global logger -----
Logger *Logger::buildGlobalLogger(Properties const &p) noexcept {
    if (Logger::globalLogger != nullptr)
        Logger::deleteGlobalLogger();
    Logger::globalLogger = new Logger(p);
    return Logger::globalLogger;
}

Logger *Logger::buildGlobalLogger(std::string const &fname, const bool append,
                                  Properties const &p) noexcept {
    if (Logger::globalLogger != nullptr)
        Logger::deleteGlobalLogger();
    Logger::globalLogger = new Logger(fname, append, p);
    return Logger::globalLogger;
}

Logger::Status Logger::deleteGlobalLogger() noexcept {
    if (Logger::globalLogger == nullptr) {
        return Logger::Status::INVALID_USE;
    }

    delete Logger::globalLogger;
    Logger::globalLogger = nullptr;
    return Logger::Status::OK;
}

Logger *Logger::getGlobalLogger() noexcept {
    return Logger::globalLogger;
}

    
// ----- manage log file -----
Logger::Status Logger::setLogFile(std::string const &fname, bool const app) {
    lock_guard<mutex> lg(this->lock);
    Status st = Status::OK;
    
    if (this->logFileName.size() != 0) {
        st = this->flush(true);
    }
    
    this->logFileName = fname;

    this->append = app;
    this->firstWrite = true;
    
    return st;
}

std::string Logger::getLogFile() {
    lock_guard<mutex> lg(this->lock);
    return this->logFileName;
}

Logger::Status Logger::prepareLogFile(std::string const &logName, bool const skipLock) {
    if (!skipLock)
        this->lock.lock();

    if (this->logFileName.size() == 0)
        return Status::NO_LOG_FILE;

    // store array of dashes
    char *dashes;
    size_t length;
    if (logName.size() != 0)
        // length is max of lengths of name and time string plus 5 spaces on either side
        length = max(logName.size(), static_cast<size_t>(19)) + static_cast<size_t>(10);
    else
        length = 19+10;

    dashes = new char[length+1];
    fill_n(dashes, length, '-');
    dashes[length] = '\0';

    // open file
    ofstream ofs;
    if (this->append)
        ofs.open(this->logFileName, ios::out|ios::app);
    else
        ofs.open(this->logFileName, ios::out|ios::trunc);
    if (!ofs.is_open()) {
        int errnum = errno;
        cerr << "Logger: Could not open log file '" << this->logFileName << "': "
             << strerror(errnum) << endl;
        if (!skipLock)
            this->lock.unlock();
        return Status::OP_FAILED;
    }

    // write header
    if (this->append)
        ofs << endl;
    ofs << dashes << endl;
    if (logName.size() != 0)
        ofs << "     " << logName << endl;
    ofs << "     " << makeDateTimeString() << endl;
    ofs << dashes << endl;

    if (ofs.fail()) {
        int errnum = errno;
        cerr << "Logger: Error writing to log file '" << this->logFileName << "': "
             << strerror(errnum) << endl;
        if (!skipLock)
            this->lock.unlock();
        return Status::OP_FAILED;
    }
    
    ofs.close();
    this->firstWrite = false;
    delete[] dashes;
    if (!skipLock)
        this->lock.unlock();
    return Status::OK;
}

Logger::Status Logger::flush(bool const skipLock) {
    if (this->logFileName.size() == 0)
        return Status::NO_LOG_FILE;

    if (this->messages.size() == 0)
        return Status::OK;
    
    if (!skipLock)
        this->lock.lock();

    if (this->firstWrite)
        this->prepareLogFile(std::string(), true);

    // open file
    ofstream ofs(this->logFileName, ios::out|ios::app);
    if (!ofs.is_open()) {
        int errnum = errno;
        cerr << "Logger: Could not open log file '" << this->logFileName << "': "
             << strerror(errnum) << endl;
        if (!skipLock)
            this->lock.unlock();
        return Status::OP_FAILED;
    }

    // write messages
    string str;
    while (!this->messages.empty()) {
        str = this->messages.front();
        ofs << str << endl;
        this->messages.pop();

        if (ofs.fail()) {
            int errnum = errno;
            cerr << "Logger: Error writing to log file '" << this->logFileName << "': "
                 << strerror(errnum) << endl;
            if (!skipLock)
                this->lock.unlock();
            return Status::OP_FAILED;
        }
    }

    ofs.close();
    if (!skipLock)
        this->lock.unlock();
    return Status::OK;
}


// ----- interface for raw messages -----
Logger::Status Logger::reportRaw(std::string const &message, int const stream) {
    Status st;
    st = this->showRaw(message, stream);
    if (this->logFileName.size() != 0) {
        if (st == Status::OK)
            st = this->logRaw(message);
        else
            this->logRaw(message);
    }
    else
        return Status::NO_LOG_FILE;
    return st;
}

Logger::Status Logger::showRaw(std::string const &message, int const stream) {
    lock_guard<mutex> lg(this->lock);
    if (stream == STDOUT_FILENO) {
        cout << message << endl;
        cout.flush();
    }
    else if (stream == STDERR_FILENO) {
        cerr << message << endl;
        cerr.flush();
    }
    else {
        cerr << "Logger::showRaw: Unknown stream: " << stream << endl;
        return Status::INVALID_USE;
    }
    return Status::OK;
}

Logger::Status Logger::logRaw(std::string const &message) {
    lock_guard<mutex> lg(this->lock);
    this->messages.push(message);
    if (this->messages.size() >= this->maxQueueLength)
        return this->flush(true);
    return Status::OK;
}


// ----- interface for messages -----
Logger::Status Logger::reportMessage(std::string const &file, unsigned int const line,
                                     std::string const &function, std::string const &message,
                                     Properties const *props) {

    Status st = Status::OK;
    this->showMessage(file, line, function, message, props);
    if (this->logFileName.size() != 0)
        st = this->logMessage(file, line, function, message, props);
    else
        return Status::NO_LOG_FILE;
    return st;
}

void Logger::showMessage(std::string const &file, unsigned int const line,
                         std::string const &function, std::string const &message,
                         Properties const *props) {

    lock_guard<mutex> lg(this->lock);
    cout << this->composeMessage(file, line, function, message, false, false, props) << endl;
    cout.flush();
}

Logger::Status Logger::logMessage(std::string const &file, unsigned int const line,
                                  std::string const &function, std::string const &message,
                                  Properties const *props) {

    lock_guard<mutex> lg(this->lock);
    this->messages.push(this->composeMessage(file, line, function, message, false, true, props));
    if (this->messages.size() >= this->maxQueueLength)
        return this->flush(true);
    return Status::OK;
}


// ----- interface for errors -----
Logger::Status Logger::reportError(std::string const &file, unsigned int const line,
                                   std::string const &function, std::string const &message,
                                   Properties const *props) {

    Status st = Status::OK;
    this->showError(file, line, function, message, props);
    if (this->logFileName.size() != 0)
        st = this->logError(file, line, function, message, props);
    else
        return Status::NO_LOG_FILE;
    return st;
}

void Logger::showError(std::string const &file, unsigned int const line,
                       std::string const &function, std::string const &message,
                       Properties const *props) {

    lock_guard<mutex> lg(this->lock);
    cerr << this->composeMessage(file, line, function, message, true, false, props) << endl;
    cerr.flush();
}

Logger::Status Logger::logError(std::string const &file, unsigned int const line,
                                std::string const &function, std::string const &message,
                                Properties const *props) {

    lock_guard<mutex> lg(this->lock);
    this->messages.push(this->composeMessage(file, line, function, message, true, true, props));
    if (this->messages.size() >= this->maxQueueLength)
        return this->flush(true);
    return Status::OK;
}


// ----- miscellanious -----
std::string Logger::makeLogName(std::string const &name) {
    string timeStr = makeDateTimeString();
    // replace all ':' by '-' and '|' by 'T'
    // (better suited for file names)
    for (size_t i = 0; i < timeStr.size(); i++) {
        if (timeStr[i] == ':')
            timeStr[i] = '-';
        else if (timeStr[i] == '|')
            timeStr[i] = 'T';
    }
    
    ostringstream ss;
    if (name.size() != 0)
        ss << name << "_";
    ss << timeStr << ".log";
    return ss.str();
}

void Logger::setMaxQueueLength(unsigned int const len) noexcept {
    this->maxQueueLength = len;
}

unsigned int Logger::getMaxQueueLength() const noexcept {
    return this->maxQueueLength;
}

Logger::Properties Logger::getProperties() noexcept {
    lock_guard<mutex> lg(this->lock);
    return this->outputProps;
}

void Logger::setProperties(Properties const &props) {
    lock_guard<mutex> lg(this->lock);
    this->outputProps = props;
}


// ----- private -----
string Logger::composeMessage(std::string const &file, unsigned int const line,
                              std::string const &function, std::string const &message,
                              bool const error, bool const toFile,
                              Properties const * props) const {

    // designated output stream, only relevant when colour==true
    int stream;
    if (error)
        stream = STDERR_FILENO;
    else
        stream = STDOUT_FILENO;

    // use default props if none given
    if (props == nullptr)
        props = &this->outputProps;

    bool const colour = !toFile && (props->flags & coloured);

    // find maximum line length if needed
    unsigned short maxLineLength = 80;
    if (toFile && props->flags & breakLinesFile) {
        if (props->maxLineLengthFile == 0) {  // use value for TTY
            maxLineLength = props->maxLineLengthTTY;
            if (!maxLineLength)
                maxLineLength = getTerminalWidth();
        }
        else
            maxLineLength = props->maxLineLengthFile;
    }
    else if (!toFile && props->flags & breakLinesTTY) {
        maxLineLength = props->maxLineLengthTTY;
        if (!maxLineLength)
                maxLineLength = getTerminalWidth();
    }
    
    ostringstream ss;    // format message
    string SGR;          // aux string for terminal SGRs
    unsigned short indentWidth;
    string indent;       // spaces in front of each line
    string extraIndent;  // extra spaces to align multi line messages
    unsigned int lengthSGRs = 0;

    if ((indentWidth = props->indent) != 0) {
        indent = string(props->indent, ' ');
        maxLineLength -= indentWidth;
        ss << indent;
    }
    
    // insert date and time
    if (toFile && (props->flags & logDate || props->flags & logTime)) {
        ss << "(" << makeDateTimeString(props->flags & logDate, props->flags & logTime)
           << ") ";
    }
    
    // insert "ERROR"
    if (error) {
        if (colour) {
            SGR = shellColourCode({Colour::RED, true, Colour::DEFAULT, false, TextProperties::NORMAL},
                                  stream);
            lengthSGRs += SGR.size();
            ss << SGR;
        }
        ss << " ERROR  ";
        if (colour) {
            SGR = shellColourCode({}, stream);
            lengthSGRs += SGR.size();
            ss << SGR;
        }

    }

    // insert file and line
    if (file.size() != 0) {
        ss << "[";
        // file
        if (colour) {
            SGR = shellColourCode({Colour::YELLOW}, stream);
            lengthSGRs += SGR.size();
            ss << SGR;
        }
        ss << file;
        if (colour) {
            SGR = shellColourCode({}, stream);
            lengthSGRs += SGR.size();
            ss << SGR;
        }
        ss << " | ";
        // line
        if (colour) {
            SGR = shellColourCode({Colour::GREEN}, stream);
            lengthSGRs += SGR.size();
            ss << SGR;
        }
        ss << line;
        if (colour) {
            SGR = shellColourCode({}, stream);
            lengthSGRs += SGR.size();
            ss << SGR;
        }
        
        // separator or end
        if (function.size() != 0)
            ss << " | ";
        else
            ss << "]: ";
    }

    // insert function name
    if (function.size() != 0) {
        if (file.size() == 0)
            ss << "[";
        ss << function << "()]: ";
    }

    // store extra indentation
    unsigned short extraIndentWidth = static_cast<unsigned short>(ss.str().size()
                                                                  - props->indent
                                                                  - lengthSGRs);
    if (extraIndentWidth > maxLineLength*2/3)
        extraIndentWidth = maxLineLength*1/3;  // make sure there is some space to print
    if (props->flags & doExtraIndent) {
        extraIndent = string(extraIndentWidth, ' ');
    }

    istringstream iss(message);
    string lineStr;
    bool firstLine = true, done;
    size_t lineLength, startI;
    unsigned short al;  // available length

    // break lines that are too long
    if ((toFile && props->flags & breakLinesFile)
        || (!toFile && props->flags & breakLinesTTY)) {

        // break lines at '\n'
        while (std::getline(iss, lineStr, '\n')) {
            lineLength = lineStr.size();
            startI = 0;
            done = false;

            // line with file, line, and func tags
            if (firstLine) {
                al = maxLineLength - extraIndentWidth;
                if (lineLength <= al) {  // message fits into line
                    ss << lineStr;
                    done = true;  // no need to process line any further
                }
                else {                   // need to split message
                    ss << lineStr.substr(0,al);
                    startI = al;
                }
                firstLine = false;
            }
                        
            if (!done) {
                // determine available length (maxLineLength is corrected for normal indent)
                if (props->flags & doExtraIndent)
                    al = maxLineLength - extraIndentWidth;
                else
                    al = maxLineLength;
            
                // continue splitting while necessary
                while (startI < lineLength && lineLength-startI > al) {
                    ss << endl << indent << extraIndent << lineStr.substr(startI,al);
                    startI += al;
                }
                // write remainder that fits into line
                if (startI < lineLength)
                    ss << endl << indent << extraIndent << lineStr.substr(startI);
            }
        }

    }
    else {  // no extra line breaking, only at '\n'
        while (std::getline(iss, lineStr, '\n')) {
            if (firstLine) {
                ss << lineStr;
                firstLine = false;
            } else {
                ss << endl << indent << extraIndent << lineStr;
            }
        }
    }
    
        
    return ss.str();
}
