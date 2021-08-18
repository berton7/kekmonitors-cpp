#pragma once
#include <kekmonitors/core.hpp>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

namespace kekmonitors {

class IMessage {
  public:
    virtual json toJson() const = 0;
    virtual std::string toString() const = 0;
};

class Cmd : public IMessage {
  protected:
    kekmonitors::CommandType m_cmd;
    json m_payload;

  public:
    Cmd();
    ~Cmd();

    static Cmd fromJson(const json &obj);
    static Cmd fromJson(const json &obj, error_code &ec);
    json toJson() const override;
    static Cmd fromString(const std::string &str);
    static Cmd fromString(const std::string &str, error_code &ec);
    std::string toString() const override;

    kekmonitors::CommandType cmd() const;
    void setCmd(kekmonitors::CommandType cmd);
    const json &payload() const;
    void setPayload(const json &payload);
};

class Response : public IMessage {
  private:
    kekmonitors::ErrorType m_error;
    std::string m_info;
    json m_payload;

  public:
    Response();
    ~Response();

    static Response fromJson(const json &obj);
    static Response fromJson(const json &obj, error_code &ec);
    json toJson() const override;
    static Response fromString(const std::string &str);
    static Response fromString(const std::string &str, error_code &ec);
    std::string toString() const override;

    kekmonitors::ErrorType error() const;
    void setError(kekmonitors::ErrorType error);
    const json &payload() const;
    void setPayload(const json &payload);
    const std::string &info() const;
    void setInfo(const std::string &info);

    static Response okResponse();
    static Response badResponse();
};
} // namespace kekmonitors