#pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <string>
#include <mutex>
#include <memory>

class Logger {
public:
    // 日志级别枚举
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    // 单例模式获取实例
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    // 初始化日志系统
    bool init(const std::string& filename, Level level = Level::INFO) {
        try {
            m_logFile.open(filename, std::ios::app);
            if (!m_logFile.is_open()) {
                return false;
            }
            m_logLevel = level;
            
            // 保存标准输出和错误输出的缓冲区
            m_oldCout = std::cout.rdbuf();
            m_oldCerr = std::cerr.rdbuf();
            
            // 重定向标准输出和错误输出到日志文件
            std::cout.rdbuf(m_logFile.rdbuf());
            std::cerr.rdbuf(m_logFile.rdbuf());
            
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    // 原有的单参数日志方法
    void log(Level level, const std::string& message) {
        if (level < m_logLevel) return;

        std::lock_guard<std::mutex> lock(m_mutex);
        m_logFile << getCurrentTimestamp() << " [" << getLevelString(level) << "] "
                  << message << std::endl;
        m_logFile.flush();
    }

    // 新增：处理多参数的模板方法
    template<typename... Args>
    void logStream(Level level, const Args&... args) {
        if (level < m_logLevel) return;

        std::ostringstream oss;
        (oss << ... << args);  // C++17 折叠表达式
        log(level, oss.str());
    }

    // 原有的便捷单参数方法
    void debug(const std::string& message) { log(Level::DEBUG, message); }
    void info(const std::string& message) { log(Level::INFO, message); }
    void warning(const std::string& message) { log(Level::WARNING, message); }
    void error(const std::string& message) { log(Level::ERROR, message); }
    void fatal(const std::string& message) { log(Level::FATAL, message); }

    // 新增：便捷的多参数模板方法
    template<typename... Args>
    void debug(const Args&... args) { logStream(Level::DEBUG, args...); }
    
    template<typename... Args>
    void info(const Args&... args) { logStream(Level::INFO, args...); }
    
    template<typename... Args>
    void warning(const Args&... args) { logStream(Level::WARNING, args...); }
    
    template<typename... Args>
    void error(const Args&... args) { logStream(Level::ERROR, args...); }

    template<typename... Args>
    void fatal(const Args&... args) { logStream(Level::FATAL, args...); }

    // 设置日志级别
    void setLevel(Level level) {
        m_logLevel = level;
    }

    // 析构函数中恢复标准输出
    ~Logger() {
        if (m_logFile.is_open()) {
            // 恢复标准输出和错误输出
            std::cout.rdbuf(m_oldCout);
            std::cerr.rdbuf(m_oldCerr);
            m_logFile.close();
        }
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // 获取当前时间戳
    std::string getCurrentTimestamp() {
        auto now = std::time(nullptr);
        auto* timeinfo = std::localtime(&now);
        char buffer[80];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
        return std::string(buffer);
    }

    // 将日志级别转换为字符串
    std::string getLevelString(Level level) {
        switch (level) {
            case Level::DEBUG: return "DEBUG";
            case Level::INFO: return "INFO";
            case Level::WARNING: return "WARNING";
            case Level::ERROR: return "ERROR";
            case Level::FATAL: return "FATAL";
            default: return "UNKNOWN";
        }
    }

    std::ofstream m_logFile;
    Level m_logLevel{Level::INFO};
    std::mutex m_mutex;
    std::streambuf* m_oldCout{nullptr};
    std::streambuf* m_oldCerr{nullptr};
};