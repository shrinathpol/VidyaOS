#include <napi.h>
#include <iostream>

// This function simulates setting the hardware resolution by bridging
// Node.js into the VidyaOS C++ core logic (or SDL2 engine).
Napi::Value SetResolution(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2 || !info[0].IsNumber() || !info[1].IsNumber()) {
        Napi::TypeError::New(env, "Number expected for width and height").ThrowAsJavaScriptException();
        return env.Null();
    }

    int width = info[0].As<Napi::Number>().Int32Value();
    int height = info[1].As<Napi::Number>().Int32Value();

    // Here we would typically interface with our existing C++ engine.
    // E.g. call a global C++ function like: vidyaos::Display::setResolution(width, height)
    // For this blueprint, we simulate the hook:
    std::cout << "[VidyaOS C++ Engine] Hardware resolution changed natively to " 
              << width << "x" << height << "!" << std::endl;

    return Napi::Boolean::New(env, true);
}

// Initialize the Node.js Addon exports
Napi::Object Init(Napi::Env env, Napi::Object exports) {
    exports.Set(Napi::String::New(env, "setResolution"),
                Napi::Function::New(env, SetResolution));
    return exports;
}

NODE_API_MODULE(resolution, Init)
