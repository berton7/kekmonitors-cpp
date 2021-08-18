#include <iostream>
#include <kekmonitors/msg.hpp>
#include <kekmonitors/utils.hpp>

namespace kekmonitors {

Cmd::Cmd() : m_cmd(COMMANDS::PING){};
Cmd::~Cmd() = default;

Cmd Cmd::fromJson(const json &obj) {
    Cmd cmd;
    bool success = false;
    try {
        cmd.m_cmd = obj.at("_Cmd__cmd");
    } catch (json::exception &e) {
        throw std::invalid_argument("Json object doesn't contain \"_Cmd__cmd\"");
    }
    try {
        cmd.m_payload = obj.at("_Cmd__payload");
    } catch (json::exception &e) {
    };
    return cmd;
};

Cmd Cmd::fromJson(const json &obj, error_code &ec)
{
    Cmd cmd;
    try {
        cmd.m_cmd = obj.at("_Cmd__cmd");
    } catch (json::exception &e) {
        ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
    }
    try {
        cmd.m_payload = obj.at("_Cmd__payload");
    } catch (json::exception &e) {
    };
    return cmd;
}

json Cmd::toJson() const {
    json j;
    j["_Cmd__cmd"] = m_cmd;
    if (!m_payload.empty())
        j["_Cmd__payload"] = m_payload;
    return j;
};

Cmd Cmd::fromString(const std::string &str) {
    return fromJson(json::parse(str));
}

Cmd Cmd::fromString(const std::string &str, error_code &ec) {
    try {
        return fromJson(json::parse(str), ec);
    }
    catch(json::exception &e)
    {
        ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
        return Cmd();
    }
}

std::string Cmd::toString() const { return toJson().dump(); }

kekmonitors::CommandType Cmd::cmd() const { return m_cmd; }
void Cmd::setCmd(kekmonitors::CommandType cmd) { m_cmd = cmd; }
const json &Cmd::payload() const { return m_payload; }
void Cmd::setPayload(const json &payload) { m_payload = payload; }

Response::Response() : m_error(ERRORS::OK){};
Response::~Response() = default;

Response Response::fromJson(const json &obj) {
    Response response;
    try {
        response.m_error = obj.at("_Response__error");
    } catch (json::exception &e) {
        throw std::invalid_argument("Json object doesn't contain \"_Response__error\"");
    };
    try {
        response.m_payload = obj.at("_Response__payload");
    } catch (json::exception &e) {
    };
    try {
        response.m_info = obj.at("_Response__info");
    } catch (json::exception &e) {
    };
    return response;
};

Response Response::fromJson(const json &obj, error_code &ec) {
    Response response;
    try {
        response.m_error = obj.at("_Response__error");
    } catch (json::exception &e) {
        ec = boost::system::errc::make_error_code(boost::system::errc::invalid_argument);
    };
    try {
        response.m_payload = obj.at("_Response__payload");
    } catch (json::exception &e) {
    };
    try {
        response.m_info = obj.at("_Response__info");
    } catch (json::exception &e) {
    };
    return response;
};

json Response::toJson() const {
    json j;
    j["_Response__error"] = m_error;
    if (!m_info.empty())
        j["_Response__info"] = m_info;
    if (!m_payload.empty())
        j["_Response__payload"] = m_payload;
    return j;
};
kekmonitors::ErrorType Response::error() const { return m_error; }
void Response::setError(kekmonitors::ErrorType error) { m_error = error; }
const json &Response::payload() const { return m_payload; }
void Response::setPayload(const json &payload) { m_payload = payload; }
const std::string &Response::info() const { return m_info; }
void Response::setInfo(const std::string &info) { m_info = info; }

Response Response::okResponse() {
    Response resp;
    resp.setError(ERRORS::OK);
    return resp;
}
Response Response::badResponse() {
    Response resp;
    resp.setError(ERRORS::OTHER_ERROR);
    return resp;
}
std::string Response::toString() const { return toJson().dump(); }
Response Response::fromString(const std::string &str) {
    return fromJson(json::parse(str));
}

Response Response::fromString(const std::string &str, error_code &ec)
{
    try {
        return fromJson(json::parse(str), ec);
    } catch (json::exception &e) {
        ec = boost::system::errc::make_error_code(
            boost::system::errc::invalid_argument);
        return Response();
    }
}
}; // namespace kekmonitors
