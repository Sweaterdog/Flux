#include "std_gpu.h"
#include "../src/interpreter.h"
#include <vector>
#include <cstring>

// ============================================================================
// std.gpu — GPU compute abstraction layer
//
// This module provides low-level GPU compute access. When CUDA or ROCm/HIP
// is available at build time, real GPU operations are used. Otherwise,
// CPU fallback stubs are provided so code runs everywhere.
//
// Compile flags:
//   -DFLUX_HAS_CUDA   -> enables CUDA backend
//   -DFLUX_HAS_ROCM   -> enables ROCm/HIP backend
//
// Provides:
//   GPU.available        -> bool
//   GPU.backend          -> string ("cuda" | "rocm" | "none")
//   GPU.deviceCount()    -> int
//   GPU.deviceName(id)   -> string
//   GPU.allocate(size)   -> GPUBuffer object
//   GPU.memcpyToDevice(buf, data_list)
//   GPU.memcpyToHost(buf) -> list
//   GPU.free(buf)
//   GPU.sync()
//
// This is intentionally "super duper low-level" — it mirrors the CUDA/HIP
// API patterns so users build higher-level abstractions in Flux on top.
// ============================================================================

#ifdef FLUX_HAS_CUDA
#include <cuda_runtime.h>
#endif

#ifdef FLUX_HAS_ROCM
#include <hip/hip_runtime.h>
#endif

void registerStdGPU(std::shared_ptr<Environment> env, Interpreter& interp) {

    auto gpuClass = std::make_shared<FluxClass>();
    gpuClass->name = "GPU";
    auto gpuObj = std::make_shared<FluxObject>();
    gpuObj->classDef = gpuClass;

    // ---------- GPU.available / GPU.available() ----------
    // Available as both a property and a function for flexibility
#if defined(FLUX_HAS_CUDA) || defined(FLUX_HAS_ROCM)
    gpuObj->fields["available"] = Value::fromBool(true);
    {
        Value fn; fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value { return Value::fromBool(true); };
        gpuObj->fields["isAvailable"] = fn;
    }
#else
    gpuObj->fields["available"] = Value::fromBool(false);
    {
        Value fn; fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value { return Value::fromBool(false); };
        gpuObj->fields["isAvailable"] = fn;
    }
#endif

    // ---------- GPU.backend ----------
#if defined(FLUX_HAS_CUDA)
    gpuObj->fields["backend"] = Value::fromString("cuda");
#elif defined(FLUX_HAS_ROCM)
    gpuObj->fields["backend"] = Value::fromString("rocm");
#else
    gpuObj->fields["backend"] = Value::fromString("none");
#endif

    // ---------- GPU.deviceCount() -> int ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
#ifdef FLUX_HAS_CUDA
            int count = 0;
            cudaGetDeviceCount(&count);
            return Value::fromInt(count);
#elif defined(FLUX_HAS_ROCM)
            int count = 0;
            hipGetDeviceCount(&count);
            return Value::fromInt(count);
#else
            return Value::fromInt(0);
#endif
        };
        gpuObj->fields["deviceCount"] = fn;
    }

    // ---------- GPU.deviceName(id) -> string ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            int id = args.empty() ? 0 : (int)args[0].toNumber();
#ifdef FLUX_HAS_CUDA
            cudaDeviceProp prop;
            if (cudaGetDeviceProperties(&prop, id) == cudaSuccess) {
                return Value::fromString(std::string(prop.name));
            }
            return Value::fromString("unknown");
#elif defined(FLUX_HAS_ROCM)
            hipDeviceProp_t prop;
            if (hipGetDeviceProperties(&prop, id) == hipSuccess) {
                return Value::fromString(std::string(prop.name));
            }
            return Value::fromString("unknown");
#else
            (void)id;
            return Value::fromString("no GPU backend");
#endif
        };
        gpuObj->fields["deviceName"] = fn;
    }

    // ---------- GPU.allocate(size_in_floats) -> GPUBuffer ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty()) throw FluxException("error", "GPU.allocate requires size argument");
            size_t count = (size_t)args[0].toNumber();
            size_t bytes = count * sizeof(float);

            auto bufClass = std::make_shared<FluxClass>();
            bufClass->name = "GPUBuffer";
            auto bufObj = std::make_shared<FluxObject>();
            bufObj->classDef = bufClass;
            bufObj->fields["size"] = Value::fromInt((int32_t)count);

#ifdef FLUX_HAS_CUDA
            float* devPtr = nullptr;
            cudaError_t err = cudaMalloc(&devPtr, bytes);
            if (err != cudaSuccess) {
                throw FluxException("error", std::string("cudaMalloc failed: ") + cudaGetErrorString(err));
            }
            bufObj->fields["_ptr"] = Value::fromLong((int64_t)(uintptr_t)devPtr);
            bufObj->fields["_backend"] = Value::fromString("cuda");
#elif defined(FLUX_HAS_ROCM)
            float* devPtr = nullptr;
            hipError_t err = hipMalloc(&devPtr, bytes);
            if (err != hipSuccess) {
                throw FluxException("error", std::string("hipMalloc failed: ") + hipGetErrorString(err));
            }
            bufObj->fields["_ptr"] = Value::fromLong((int64_t)(uintptr_t)devPtr);
            bufObj->fields["_backend"] = Value::fromString("rocm");
#else
            // CPU fallback: allocate host memory
            auto hostBuf = std::make_shared<std::vector<float>>(count, 0.0f);
            bufObj->fields["_ptr"] = Value::fromLong((int64_t)(uintptr_t)hostBuf->data());
            bufObj->fields["_backend"] = Value::fromString("cpu");
            // Store shared_ptr via a closure so the buffer lives as long as the object
            Value _ref;
            _ref.type = ValueType::NATIVE_FUNCTION;
            _ref.nativeFn = [hostBuf](Interpreter&, std::vector<Value>) -> Value { return Value::nil(); };
            bufObj->fields["_ref"] = _ref;
#endif

            Value result;
            result.type = ValueType::OBJECT;
            result.objectVal = bufObj;
            return result;
        };
        gpuObj->fields["allocate"] = fn;
    }

    // ---------- GPU.memcpyToDevice(buf, data_list) ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.size() < 2 || args[0].type != ValueType::OBJECT || args[1].type != ValueType::LIST) {
                throw FluxException("error", "GPU.memcpyToDevice requires (GPUBuffer, list)");
            }
            auto& obj = args[0].objectVal;
            auto& list = *args[1].listVal;
            size_t count = (size_t)obj->fields["size"].toNumber();
            if (list.size() > count) throw FluxException("error", "Data list exceeds buffer size");

            std::vector<float> hostData(list.size());
            for (size_t i = 0; i < list.size(); i++) {
                hostData[i] = (float)list[i].toNumber();
            }

            float* ptr = (float*)(uintptr_t)obj->fields["_ptr"].longVal;

#ifdef FLUX_HAS_CUDA
            cudaMemcpy(ptr, hostData.data(), hostData.size() * sizeof(float), cudaMemcpyHostToDevice);
#elif defined(FLUX_HAS_ROCM)
            hipMemcpy(ptr, hostData.data(), hostData.size() * sizeof(float), hipMemcpyHostToDevice);
#else
            // CPU fallback: memcpy on host
            std::memcpy(ptr, hostData.data(), hostData.size() * sizeof(float));
#endif

            return Value::nil();
        };
        gpuObj->fields["memcpyToDevice"] = fn;
    }

    // ---------- GPU.memcpyToHost(buf) -> list ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty() || args[0].type != ValueType::OBJECT) {
                throw FluxException("error", "GPU.memcpyToHost requires a GPUBuffer argument");
            }
            auto& obj = args[0].objectVal;
            size_t count = (size_t)obj->fields["size"].toNumber();
            float* ptr = (float*)(uintptr_t)obj->fields["_ptr"].longVal;

            std::vector<float> hostData(count);

#ifdef FLUX_HAS_CUDA
            cudaMemcpy(hostData.data(), ptr, count * sizeof(float), cudaMemcpyDeviceToHost);
#elif defined(FLUX_HAS_ROCM)
            hipMemcpy(hostData.data(), ptr, count * sizeof(float), hipMemcpyDeviceToHost);
#else
            std::memcpy(hostData.data(), ptr, count * sizeof(float));
#endif

            auto list = std::make_shared<std::vector<Value>>();
            for (size_t i = 0; i < count; i++) {
                list->push_back(Value::fromFloat((double)hostData[i]));
            }
            return Value::fromList(list);
        };
        gpuObj->fields["memcpyToHost"] = fn;
    }

    // ---------- GPU.free(buf) ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value> args) -> Value {
            if (args.empty() || args[0].type != ValueType::OBJECT) {
                throw FluxException("error", "GPU.free requires a GPUBuffer argument");
            }
            auto& obj = args[0].objectVal;
            float* ptr = (float*)(uintptr_t)obj->fields["_ptr"].longVal;

#ifdef FLUX_HAS_CUDA
            cudaFree(ptr);
#elif defined(FLUX_HAS_ROCM)
            hipFree(ptr);
#else
            // CPU fallback: the shared_ptr in _ref handles deallocation
            (void)ptr;
#endif

            obj->fields["_ptr"] = Value::fromLong(0);
            return Value::nil();
        };
        gpuObj->fields["free"] = fn;
    }

    // ---------- GPU.sync() ----------
    {
        Value fn;
        fn.type = ValueType::NATIVE_FUNCTION;
        fn.nativeFn = [](Interpreter&, std::vector<Value>) -> Value {
#ifdef FLUX_HAS_CUDA
            cudaDeviceSynchronize();
#elif defined(FLUX_HAS_ROCM)
            hipDeviceSynchronize();
#endif
            return Value::nil();
        };
        gpuObj->fields["sync"] = fn;
    }

    Value gpuVal;
    gpuVal.type = ValueType::OBJECT;
    gpuVal.objectVal = gpuObj;
    env->define("GPU", gpuVal, "object");
}
