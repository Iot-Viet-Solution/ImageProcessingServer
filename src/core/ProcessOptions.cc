#include "core/ProcessOptions.h"

#include <drogon/HttpRequest.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace ips {

namespace {

std::string lower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

// Returns the query parameter value, or empty string if absent.
std::string param(const drogon::HttpRequest& req, const std::string& key) {
  return req.getParameter(key);
}

int toInt(const std::string& key, const std::string& v) {
  try {
    std::size_t pos = 0;
    int out = std::stoi(v, &pos);
    if (pos != v.size()) throw std::invalid_argument("trailing");
    return out;
  } catch (const std::exception&) {
    throw std::invalid_argument("parameter '" + key + "' must be an integer");
  }
}

double toDouble(const std::string& key, const std::string& v) {
  try {
    std::size_t pos = 0;
    double out = std::stod(v, &pos);
    if (pos != v.size()) throw std::invalid_argument("trailing");
    return out;
  } catch (const std::exception&) {
    throw std::invalid_argument("parameter '" + key + "' must be a number");
  }
}

Fit parseFit(const std::string& v) {
  std::string f = lower(v);
  if (f == "inside") return Fit::Inside;
  if (f == "outside") return Fit::Outside;
  if (f == "cover") return Fit::Cover;
  if (f == "fill") return Fit::Fill;
  throw std::invalid_argument("fit must be one of: inside, outside, cover, fill");
}

}  // namespace

ProcessOptions ProcessOptions::fromRequest(const drogon::HttpRequest& req) {
  ProcessOptions o;

  if (auto v = param(req, "w"); !v.empty()) {
    o.width = toInt("w", v);
    if (*o.width <= 0) throw std::invalid_argument("w must be > 0");
  }
  if (auto v = param(req, "h"); !v.empty()) {
    o.height = toInt("h", v);
    if (*o.height <= 0) throw std::invalid_argument("h must be > 0");
  }
  if (auto v = param(req, "fit"); !v.empty()) {
    o.fit = parseFit(v);
  }

  if (auto v = param(req, "crop"); !v.empty()) {
    // crop=x,y,w,h
    CropRect c{};
    int* fields[] = {&c.x, &c.y, &c.w, &c.h};
    std::size_t idx = 0, start = 0;
    for (std::size_t i = 0; i <= v.size(); ++i) {
      if (i == v.size() || v[i] == ',') {
        if (idx >= 4) throw std::invalid_argument("crop expects x,y,w,h");
        *fields[idx++] = toInt("crop", v.substr(start, i - start));
        start = i + 1;
      }
    }
    if (idx != 4) throw std::invalid_argument("crop expects x,y,w,h");
    if (c.w <= 0 || c.h <= 0) throw std::invalid_argument("crop w/h must be > 0");
    o.crop = c;
  }

  if (auto v = param(req, "rotate"); !v.empty()) {
    o.rotate = toInt("rotate", v);
  }
  if (auto v = param(req, "flip"); !v.empty()) {
    o.flip = lower(v);
    if (o.flip != "h" && o.flip != "v" && o.flip != "hv" && o.flip != "vh") {
      throw std::invalid_argument("flip must be one of: h, v, hv");
    }
  }
  if (auto v = param(req, "grayscale"); !v.empty()) {
    std::string g = lower(v);
    o.grayscale = (g == "1" || g == "true" || g == "yes");
  }
  if (auto v = param(req, "blur"); !v.empty()) {
    o.blur = toDouble("blur", v);
    if (*o.blur < 0) throw std::invalid_argument("blur must be >= 0");
  }

  if (auto v = param(req, "format"); !v.empty()) {
    o.format = lower(v);
  }
  if (auto v = param(req, "q"); !v.empty()) {
    o.quality = toInt("q", v);
    if (*o.quality < 1 || *o.quality > 100) {
      throw std::invalid_argument("q (quality) must be between 1 and 100");
    }
  }

  return o;
}

}  // namespace ips
