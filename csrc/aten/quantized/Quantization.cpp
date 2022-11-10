#include <ATen/InitialTensorOptions.h>
#include <ATen/native/TensorFactories.h>
#include <ATen/native/TensorIterator.h>
#include <ATen/quantized/QTensorImpl.h>
#include <core/TensorImplUtils.h>
#include <core/detail/ListUtils.h>
#include <oneDNN/oneDNN.h>
#include <quantized/QTensor.h>
#include <quantized/Quantization.h>
#include <quantized/Quantizer.h>
#include <utils/DPCPP.h>

using namespace at::native;
using namespace xpu::dpcpp;

namespace at {
namespace AtenIpexTypeXPU {

using namespace at::AtenIpexTypeQuantizedXPU;

Tensor quantize_tensor_per_channel_affine(
    Tensor& qtensor,
    const Tensor& rtensor,
    const Tensor& scales,
    const Tensor& zero_points,
    int64_t axis) {
  xpu::oneDNN::ReorderAttr rattr = xpu::oneDNN::ReorderAttr();
  int mask_0 = 1 << axis;
  int mask_1 = 0;
  std::vector<float> scls;
  std::vector<int> zps;
  for (int i = 0; i < scales.numel(); i++) {
    scls.push_back(1.0f / scales[i].item().to<float>());
  }
  zps.push_back(zero_points[0].item().to<float>());

  rattr.set_dst_sc_and_zp(mask_0, scls, mask_1, zps);
  xpu::oneDNN::reorder(rtensor, qtensor, rattr);

  return qtensor;
}

static inline memory::format_tag quant_dst_fmt(
    const int64_t ndim,
    const bool is_channels_last = false) {
  if (!is_channels_last) {
    return (ndim == 3)
        ? memory::format_tag::ncw
        : ((ndim == 4) ? memory::format_tag::nchw
                       : ((ndim == 5) ? memory::format_tag::ncdhw
                                      : memory::format_tag::undef));
  } else {
    return (ndim == 3)
        ? memory::format_tag::nwc
        : ((ndim == 4) ? memory::format_tag::nhwc
                       : ((ndim == 5) ? memory::format_tag::ndhwc
                                      : memory::format_tag::undef));
  }
}

Tensor quantize_tensor_per_tensor_affine(
    Tensor& qtensor,
    const Tensor& rtensor,
    double scale,
    int64_t zero_point) {
  auto r_ctx = at::AtenIpexTypeXPU::DPCPPTensorContext::get_tensor_ctx(rtensor);
  if (Settings::I().is_onednn_layout_enabled()) {
    // this is a temporary implementation for forcing linear to fp32 path
    // and get better performance,due to oneDNN int8 kernel slower than fp32
    // kernel by currently.
    if (rtensor.dim() == 2) {
      return rtensor;
    }
    // for int8 weight cache
    if (!r_ctx.is_plain() &&
        (r_ctx.meta().data_type() != memory::data_type::f32)) {
      return rtensor;
    }
  }

  xpu::oneDNN::ReorderAttr rattr = xpu::oneDNN::ReorderAttr();
  int mask = 0;
  std::vector<float> scls = {static_cast<float>(1.0f / scale)};
  std::vector<int> zps = {static_cast<int>(0)};

  rattr.set_dst_sc_and_zp(mask, scls, mask, zps);
  if (qtensor.scalar_type() == kQUInt8 && zero_point == 128) {
    Tensor qtensor_opt = qtensor;
    memory::dims q_dims = rtensor.dim() == 5
        ? memory::dims(
              {rtensor.size(0),
               rtensor.size(1),
               rtensor.size(2),
               rtensor.size(3),
               rtensor.size(4)})
        : rtensor.dim() == 4 ? memory::dims(
                                   {rtensor.size(0),
                                    rtensor.size(1),
                                    rtensor.size(2),
                                    rtensor.size(3)})
                             : rtensor.dim() == 2
                ? memory::dims({rtensor.size(0), rtensor.size(1)})
                : memory::dims({rtensor.size(0)});

    memory::format_tag q_fmt =
        quant_dst_fmt(rtensor.dim(), is_smf_channels_last(rtensor));

    // We will force to specify s8 as quantization data type to meet the
    // requirement of pytorch calibration with unified data type.
    memory::data_type q_dt =
        (qtensor.scalar_type() == kQUInt8 && zero_point == 128)
        ? memory::data_type::s8
        : xpu::oneDNN::get_onednn_dtype(qtensor);
    memory::desc q_md = memory::desc(q_dims, q_dt, q_fmt);
    auto q_type = (qtensor.scalar_type() == kQUInt8 && zero_point == 128)
        ? kQInt8
        : qtensor.scalar_type();
    auto quantizer = dpcpp_make_per_tensor_affine_quantizer(
        scale,
        0,
        (xpu::oneDNN::get_onednn_dtype(qtensor) == memory::data_type::u8 &&
         zero_point == 128)
            ? kQInt8
            : q_type);
    qtensor_opt =
        AtenIpexTypeXPU::empty_opaque_qtensor(q_md, c10::nullopt, quantizer);

    xpu::oneDNN::reorder(rtensor, qtensor_opt, rattr);
    auto q_opt_ctx =
        at::AtenIpexTypeXPU::DPCPPTensorContext::release_tensor_ctx(
            qtensor_opt);
    at::AtenIpexTypeXPU::DPCPPTensorContext::set_tensor_ctx(
        qtensor, std::move(q_opt_ctx));
  } else {
    xpu::oneDNN::reorder(rtensor, qtensor, rattr);
  }

  return qtensor;
}

Tensor quantize_per_tensor(
    const Tensor& self,
    double scale,
    int64_t zero_point,
    ScalarType dtype) {
  if (self.is_quantized()) {
    return self;
  }
  auto quantizer =
      dpcpp_make_per_tensor_affine_quantizer(scale, zero_point, dtype);
  return quantizer->quantize(self);
}

Tensor quantize_per_tensor(
    const Tensor& self,
    const Tensor& scale,
    const Tensor& zero_point,
    ScalarType dtype) {
  if (self.is_quantized()) {
    return self;
  }

  auto quantizer = dpcpp_make_per_tensor_affine_quantizer(
      scale.item().toDouble(), zero_point.item().toLong(), dtype);
  return quantizer->quantize(self);
}

Tensor quantize_per_channel(
    const Tensor& self,
    const Tensor& scales,
    const Tensor& zero_points,
    int64_t axis,
    ScalarType dtype) {
  auto quantizer =
      dpcpp_make_per_channel_affine_quantizer(scales, zero_points, axis, dtype);
  return quantizer->quantize(self);
}

Tensor _empty_affine_quantized(
    IntArrayRef size,
    c10::optional<at::ScalarType> dtype,
    c10::optional<at::Layout> layout,
    c10::optional<at::Device> device,
    c10::optional<bool> pin_memory,
    double scale,
    int64_t zero_point,
    c10::optional<c10::MemoryFormat> optional_memory_format) {
  TensorOptions options_ =
      TensorOptions().dtype(dtype).layout(layout).device(device).pinned_memory(
          pin_memory);
  TORCH_CHECK(
      !(options_.has_memory_format() && optional_memory_format.has_value()),
      "Cannot set memory_format both in TensorOptions and explicit argument; "
      "please delete "
      "the redundant setter.");
  auto options = options_.memory_format(optional_memory_format);
  TORCH_CHECK(
      options.has_dtype(),
      "Must provide data type for Tensor creation functions.");
  return AtenIpexTypeXPU::new_qtensor(
      size,
      options,
      dpcpp_make_per_tensor_affine_quantizer(
          scale, 0, typeMetaToScalarType(options.dtype())));
}

Tensor _empty_per_channel_affine_quantized(
    IntArrayRef size,
    const Tensor& scales,
    const Tensor& zero_points,
    int64_t axis,
    c10::optional<at::ScalarType> dtype,
    c10::optional<at::Layout> layout,
    c10::optional<at::Device> device,
    c10::optional<bool> pin_memory,
    c10::optional<c10::MemoryFormat> optional_memory_format) {
  TensorOptions options_ =
      TensorOptions().dtype(dtype).layout(layout).device(device).pinned_memory(
          pin_memory);
  TORCH_CHECK(
      !(options_.has_memory_format() && optional_memory_format.has_value()),
      "Cannot set memory_format both in TensorOptions and explicit argument; "
      "please delete "
      "the redundant setter.");
  auto options = options_.memory_format(optional_memory_format);
  TORCH_CHECK(
      options.has_dtype(),
      "Must provide data type for Tensor creation functions.");
  TORCH_CHECK(
      options.dtype() == kQInt8 || options.dtype() == kQUInt8,
      "Supported data type for tensor creation is int8 or uint8");
  return AtenIpexTypeXPU::new_qtensor(
      size,
      options,
      dpcpp_make_per_channel_affine_quantizer(
          scales, zero_points, axis, typeMetaToScalarType(options.dtype())));
}

} // namespace AtenIpexTypeXPU

namespace AtenIpexTypeQuantizedXPU {

using namespace at::AtenIpexTypeXPU;

Tensor _empty_affine_quantized(
    IntArrayRef size,
    c10::optional<at::ScalarType> dtype,
    c10::optional<at::Layout> layout,
    c10::optional<at::Device> device,
    c10::optional<bool> pin_memory,
    double scale,
    int64_t zero_point,
    c10::optional<c10::MemoryFormat> optional_memory_format) {
  TensorOptions options_ =
      TensorOptions().dtype(dtype).layout(layout).device(device).pinned_memory(
          pin_memory);
  TORCH_CHECK(
      !(options_.has_memory_format() && optional_memory_format.has_value()),
      "Cannot set memory_format both in TensorOptions and explicit argument; "
      "please delete "
      "the redundant setter.");
  auto options = options_.memory_format(optional_memory_format);
  TORCH_CHECK(
      options.has_dtype(),
      "Must provide data type for Tensor creation functions.");
  return AtenIpexTypeXPU::new_qtensor(
      size,
      options,
      dpcpp_make_per_tensor_affine_quantizer(
          scale, zero_point, typeMetaToScalarType(options.dtype())));
}

Tensor _empty_per_channel_affine_quantized(
    IntArrayRef size,
    const Tensor& scales,
    const Tensor& zero_points,
    int64_t axis,
    c10::optional<at::ScalarType> dtype,
    c10::optional<at::Layout> layout,
    c10::optional<at::Device> device,
    c10::optional<bool> pin_memory,
    c10::optional<c10::MemoryFormat> optional_memory_format) {
  TensorOptions options_ =
      TensorOptions().dtype(dtype).layout(layout).device(device).pinned_memory(
          pin_memory);
  TORCH_CHECK(
      !(options_.has_memory_format() && optional_memory_format.has_value()),
      "Cannot set memory_format both in TensorOptions and explicit argument; "
      "please delete "
      "the redundant setter.");
  auto options = options_.memory_format(optional_memory_format);
  TORCH_CHECK(
      options.has_dtype(),
      "Must provide data type for Tensor creation functions.");
  TORCH_CHECK(
      options.dtype() == kQInt8 || options.dtype() == kQUInt8,
      "Supported data type for tensor creation is int8 or uint8");
  return AtenIpexTypeXPU::new_qtensor(
      size,
      options,
      dpcpp_make_per_channel_affine_quantizer(
          scales, zero_points, axis, typeMetaToScalarType(options.dtype())));
}

Tensor quantize_per_tensor(
    const Tensor& self,
    double scale,
    int64_t zero_point,
    ScalarType dtype) {
  if (self.is_quantized()) {
    return self;
  }
  auto quantizer = dpcpp_make_per_tensor_affine_quantizer(scale, 0, dtype);
  return quantizer->quantize(self);
}

Tensor quantize_per_tensor(
    const Tensor& self,
    const Tensor& scale,
    const Tensor& zero_point,
    ScalarType dtype) {
  if (self.is_quantized()) {
    return self;
  }
  auto quantizer = dpcpp_make_per_tensor_affine_quantizer(
      scale.item().toDouble(), zero_point.item().toLong(), dtype);
  return quantizer->quantize(self);
}

Tensor quantize_per_channel(
    const Tensor& self,
    const Tensor& scales,
    const Tensor& zero_points,
    int64_t axis,
    ScalarType dtype) {
  auto quantizer =
      dpcpp_make_per_channel_affine_quantizer(scales, zero_points, axis, dtype);
  return quantizer->quantize(self);
}

} // namespace AtenIpexTypeQuantizedXPU
} // namespace at