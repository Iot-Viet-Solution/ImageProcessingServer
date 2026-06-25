#pragma once

#include <drogon/HttpController.h>

namespace ips {

// POST /v1/process
//
// Body: raw image bytes, or a multipart/form-data upload with an "image" (or
// "file") field. Transform options are supplied as query parameters:
//
//   w, h          target width / height (px)
//   fit           inside | outside | cover | fill   (default: inside)
//   crop          x,y,w,h  (applied before resize)
//   rotate        degrees clockwise
//   flip          h | v | hv
//   grayscale     1 | true
//   blur          gaussian sigma
//   format        jpeg | png | webp | avif | gif | tiff (default: keep source)
//   q             output quality 1..100 (lossy formats)
//
// Returns the processed image bytes. Nothing is ever persisted.
class ProcessController : public drogon::HttpController<ProcessController> {
 public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(ProcessController::process, "/v1/process", drogon::Post,
                drogon::Options);
  METHOD_LIST_END

  void process(const drogon::HttpRequestPtr& req,
               std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace ips
