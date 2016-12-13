#include "logger.hpp"

#include <cstring>
#include <ctime>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <algorithm>

#include "output.hpp"

using namespace std;

// ----- init static member -----
Logger *Logger::globalLogger = nullptr;


// ----- ctors -----
Logger::Logger() : logFileName(nullptr), append(true), coloured(true), firstWrite(true),
                   maxQueueLength(5) {
}


Logger::Logger(const char * const fname, const bool app)
    : coloured(true), firstWrite(true), maxQueueLength(5) {
    
    this->logFileName = new char[strlen(fname)+1];
    strcpy(this->logFileName, fname);
    this->append = app;
}


// ----- dtor -----
Logger::~Logger() {
    this->flush();
}


// ----- manage global logger -----
Logger *Logger::buildGlobalLogger() noexcept {
    if (Logger::globalLogger != nullptr)
        Logger::deleteGlobalLogger();
    Logger::globalLogger = new Logger();
    return Logger::globalLogger;
}

Logger *Logger::buildGlobalLogger(const char * const fname, const bool append) noexcept {
    if (Logger::globalLogger != nullptr)
        Logger::deleteGlobalLogger();
    Logger::globalLogger = new Logger(fname, append);
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
Logger::Status Logger::setLogFile(const char * const fname, const bool app) noexcept {
    lock_guard<mutex> lg(this->lock);
    Status st = Status::OK;
    
    if (this->logFileName != nullptr) {
        st = this->flush(true);
        delete[] this->logFileName;
    }
    
    this->logFileName = new char[strlen(fname)+1];
    strcpy(this->logFileName, fname);

    this->append = app;
    this->firstWrite = true;
    
    return st;
}

char *Logger::getLogFile() noexcept {
    lock_guard<mutex> lg(this->lock);
    char *fname = new char[strlen(this->logFileName)+1];
    strcpy(fname, this->logFileName);
    return fname;
}

Logger::Status Logger::prepareLogFile(const char * const logName,
                                      const bool skipLock) noexcept {

    if (!skipLock)
        this->lock.lock();
        
    if (this->logFileName == nullptr)
        return Status::NO_LOG_FILE;

    // store array of dashes
    char *dashes;
    size_t length;
    if (logName != nullptr)
        // length is max of lengths of name and time string plus 5 spaces on either side
        length = max(strlen(logName), static_cast<size_t>(19)) + static_cast<size_t>(10);
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
    if (logName != nullptr)
        ofs << "     " << logName << endl;
    ofs << "     " << Logger::makeTimeString() << endl;
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

Logger::Status Logger::flush(const bool skipLock) noexcept {
    if (this->logFileName == nullptr)
        return Status::NO_LOG_FILE;

    if (this->messages.size() == 0)
        return Status::OK;
    
    if (!skipLock)
        this->lock.lock();
    
    if (this->firstWrite)
        this->prepareLogFile(nullptr, true);
    
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
Logger::Status Logger::reportRaw(const char * const message, const int stream) noexcept {
    Status st;
    st = this->showRaw(message, stream);
    if (this->logFileName != nullptr) {
        if (st == Status::OK)
            st = this->logRaw(message);
        else
            this->logRaw(message);
    }
    else
        return Status::NO_LOG_FILE;
    return st;
}

Logger::Status Logger::showRaw(const char * const message, const int stream) noexcept {
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
Logger::Status Logger::logRaw(const char * const message) noexcept {
    lock_guard<mutex> lg(this->lock);
    this->messages.push(string(message));
    if (this->messages.size() >= this->maxQueueLength)
        return this->flush(true);
    return Status::OK;
}


// ----- interface for messages -----
Logger::Status Logger::reportMessage(const char * const file, const unsigned int line,
                                     const char * const function,
                                     const char * const message) noexcept {

    Status st = Status::OK;
    this->showMessage(file, line, function, message);
    if (this->logFileName != nullptr)
        st = this->logMessage(file, line, function, message);
    else
        return Status::NO_LOG_FILE;
    return st;
}

void Logger::showMessage(const char * const file, const unsigned int line,
                         const char * const function,
                         const char * const message) noexcept {

    lock_guard<mutex> lg(this->lock);
    cout << this->composeMessage(file, line, function, message, false, false, this->coloured*true) << endl;
    cout.flush();
}

Logger::Status Logger::logMessage(const char * const file, const unsigned int line,
                                  const char * const function,
                                  const char * const message) noexcept {

    lock_guard<mutex> lg(this->lock);
    this->messages.push(this->composeMessage(file, line, function, message, false, true, false));
    if (this->messages.size() >= this->maxQueueLength)
        return this->flush(true);
    return Status::OK;
}


// ----- interface for errors -----
Logger::Status Logger::reportError(const char * const file, const unsigned int line,
                                     const char * const function,
                                     const char * const message) noexcept {

    Status st = Status::OK;
    this->showError(file, line, function, message);
    if (this->logFileName != nullptr)
        st = this->logError(file, line, function, message);
    else
        return Status::NO_LOG_FILE;
    return st;
}

void Logger::showError(const char * const file, const unsigned int line,
                         const char * const function,
                         const char * const message) noexcept {

    lock_guard<mutex> lg(this->lock);
    cerr << this->composeMessage(file, line, function, message, true, false, this->coloured*true) << endl;
    cerr.flush();
}

Logger::Status Logger::logError(const char * const file, const unsigned int line,
                                  const char * const function,
                                  const char * const message) noexcept {

    lock_guard<mutex> lg(this->lock);
    this->messages.push(this->composeMessage(file, line, function, message, true, true, false));
    if (this->messages.size() >= this->maxQueueLength)
        return this->flush(true);
    return Status::OK;
}


// ----- miscellanious -----
string Logger::makeLogName(const char * const name) noexcept {
    char *timeStr = Logger::makeTimeString();
    ostringstream ss;
    
    if (name != nullptr)
        ss << name << "_";

    ss << timeStr << ".log";
    return ss.str();
}

void Logger::setColoured(const bool clrd) noexcept {
    lock_guard<mutex> lg(this->lock);
    this->coloured = clrd;
}

void Logger::setMaxQueueLength(const unsigned char len) noexcept {
    this->maxQueueLength = len;
}

unsigned char Logger::getMaxQueueLength() const noexcept {
    return this->maxQueueLength;
}


// ----- private -----
char *Logger::makeTimeString() noexcept {
    char *buf = new char[20];
    time_t now = time(0);
    tm *ltm = localtime(&now);
    sprintf(buf, "%u-%02u-%02uT%02u-%02u-%02u",
            1900+ltm->tm_year, 1+ltm->tm_mon, ltm->tm_mday,
            ltm->tm_hour, ltm->tm_min, ltm->tm_sec);
    return buf;
}

string Logger::composeMessage(const char * const file, const unsigned int line,
                              const char * const function, const char * const message,
                              const bool error, const bool insertTime, const bool colour)
    const noexcept {

    ostringstream ss;
    // designated output stream, only relevant when colour==true
    int stream;
    if (error)
        stream = STDERR_FILENO;
    else
        stream = STDOUT_FILENO;

    // insert date and time
    if (insertTime) {
        char *buf = Logger::makeTimeString();
        ss << "(" << buf << ") ";
        delete[] buf;
    }

    // insert "ERROR"
    if (error) {
        if (colour)
            ss << shellColourCode({Colour::RED, true, Colour::DEFAULT, false, TextProperties::NORMAL},
                                  stream);
        ss << " ERROR  ";
        if (colour)
            ss << shellColourCode({}, stream);
    }

    // insert file and line
    if (file != nullptr) {
        ss << "[";
        // file
        if (colour)
            ss << shellColourCode({Colour::YELLOW}, stream);
        ss << file;
        if (colour)
            ss << shellColourCode({}, stream);
        ss << " | ";
        // line
        if (colour)
            ss << shellColourCode({Colour::GREEN}, stream);
        ss << line;
        if (colour)
            ss << shellColourCode({}, stream);
        // separator or end
        if (function != nullptr)
            ss << " | ";
        else
            ss << "]: ";
    }

    // insert function
    if (function != nullptr) {
        if (file == nullptr)
            ss << "[";
        ss << function << "()]: ";
    }

    ss << message;

    return ss.str();
}
