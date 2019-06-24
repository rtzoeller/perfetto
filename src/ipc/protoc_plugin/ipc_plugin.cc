/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <functional>
#include <memory>
#include <set>
#include <string>

#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/cpp/cpp_options.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/strutil.h>

namespace perfetto {
namespace ipc {
namespace {

using google::protobuf::FileDescriptor;
using google::protobuf::MethodDescriptor;
using google::protobuf::ServiceDescriptor;
using google::protobuf::Split;
using google::protobuf::StripString;
using google::protobuf::StripSuffixString;
using google::protobuf::UpperString;
using google::protobuf::compiler::GeneratorContext;
using google::protobuf::io::Printer;
using google::protobuf::io::ZeroCopyOutputStream;

static const char kBanner[] = "// DO NOT EDIT. Autogenerated by Perfetto IPC\n";

static const char kHeaderSvcClass[] = R"(
class $c$ : public ::perfetto::ipc::Service {
 private:
  static ::perfetto::ipc::ServiceDescriptor* NewDescriptor();

 public:
  ~$c$() override;

  static const ::perfetto::ipc::ServiceDescriptor& GetDescriptorStatic();

  // Service implementation.
  const ::perfetto::ipc::ServiceDescriptor& GetDescriptor() override;

  // Methods from the .proto file
)";

static const char kHeaderProxyClass[] = R"(
class $c$Proxy : public ::perfetto::ipc::ServiceProxy {
 public:
   explicit $c$Proxy(::perfetto::ipc::ServiceProxy::EventListener*);
   ~$c$Proxy() override;

  // ServiceProxy implementation.
  const ::perfetto::ipc::ServiceDescriptor& GetDescriptor() override;

  // Methods from the .proto file
)";

static const char kCppClassDefinitions[] = R"(
const ::perfetto::ipc::ServiceDescriptor& $c$::GetDescriptorStatic() {
  static auto* instance = NewDescriptor();
  return *instance;
}

// Host-side definitions.
$c$::~$c$() = default;

const ::perfetto::ipc::ServiceDescriptor& $c$::GetDescriptor() {
  return GetDescriptorStatic();
}

// Client-side definitions.
$c$Proxy::$c$Proxy(::perfetto::ipc::ServiceProxy::EventListener* event_listener)
    : ::perfetto::ipc::ServiceProxy(event_listener) {}

$c$Proxy::~$c$Proxy() = default;

const ::perfetto::ipc::ServiceDescriptor& $c$Proxy::GetDescriptor() {
  return $c$::GetDescriptorStatic();
}
)";

static const char kCppMethodDescriptor[] = R"(
  desc->methods.emplace_back(::perfetto::ipc::ServiceDescriptor::Method{
     "$m$",
     &_IPC_Decoder<$i$>,
     &_IPC_Decoder<$o$>,
     &_IPC_Invoker<$c$, $i$, $o$, &$c$::$m$>});
)";

static const char kCppMethod[] = R"(
void $c$Proxy::$m$(const $i$& request, Deferred$o$ reply, int fd) {
  BeginInvoke("$m$", request, ::perfetto::ipc::DeferredBase(std::move(reply)),
              fd);
}
)";

std::string StripName(const FileDescriptor& file) {
  return StripSuffixString(file.name(), ".proto");
}

std::string GetStubName(const FileDescriptor& file) {
  return StripName(file) + ".ipc";
}

void ForEachMethod(const ServiceDescriptor& svc,
                   std::function<void(const MethodDescriptor&,
                                      const std::string&,
                                      const std::string&)> function) {
  for (int i = 0; i < svc.method_count(); i++) {
    const MethodDescriptor& method = *svc.method(i);
    // TODO if the input or output type are in a different namespace we need to
    // emit the ::fully::qualified::name.
    std::string input_type = method.input_type()->name();
    std::string output_type = method.output_type()->name();
    function(method, input_type, output_type);
  }
}

void GenerateServiceHeader(const FileDescriptor& file,
                           const ServiceDescriptor& svc,
                           Printer* printer) {
  printer->Print("\n");
  std::vector<std::string> namespaces = Split(file.package(), ".");
  for (const std::string& ns : namespaces)
    printer->Print("namespace $ns$ {\n", "ns", ns);

  // Generate the host-side declarations.
  printer->Print(kHeaderSvcClass, "c", svc.name());
  std::set<std::string> types_seen;
  ForEachMethod(svc, [&types_seen, printer](const MethodDescriptor& method,
                                            const std::string& input_type,
                                            const std::string& output_type) {
    if (types_seen.count(output_type) == 0) {
      printer->Print("  using Deferred$o$ = ::perfetto::ipc::Deferred<$o$>;\n",
                     "o", output_type);
      types_seen.insert(output_type);
    }
    printer->Print("  virtual void $m$(const $i$&, Deferred$o$) = 0;\n\n", "m",
                   method.name(), "i", input_type, "o", output_type);
  });
  printer->Print("};\n\n");

  // Generate the client-side declarations.
  printer->Print(kHeaderProxyClass, "c", svc.name());
  types_seen.clear();
  ForEachMethod(svc, [&types_seen, printer](const MethodDescriptor& method,
                                            const std::string& input_type,
                                            const std::string& output_type) {
    if (types_seen.count(output_type) == 0) {
      printer->Print("  using Deferred$o$ = ::perfetto::ipc::Deferred<$o$>;\n",
                     "o", output_type);
      types_seen.insert(output_type);
    }
    printer->Print("  void $m$(const $i$&, Deferred$o$, int fd = -1);\n\n", "m",
                   method.name(), "i", input_type, "o", output_type);
  });
  printer->Print("};\n\n");

  for (auto it = namespaces.rbegin(); it != namespaces.rend(); it++)
    printer->Print("}  // namespace $ns$\n", "ns", *it);

  printer->Print("\n");
}

void GenerateServiceCpp(const FileDescriptor& file,
                        const ServiceDescriptor& svc,
                        Printer* printer) {
  printer->Print("\n");

  std::vector<std::string> namespaces = Split(file.package(), ".");
  for (const std::string& ns : namespaces)
    printer->Print("namespace $ns$ {\n", "ns", ns);

  printer->Print("::perfetto::ipc::ServiceDescriptor* $c$::NewDescriptor() {\n",
                 "c", svc.name());
  printer->Print("  auto* desc = new ::perfetto::ipc::ServiceDescriptor();\n");
  printer->Print("  desc->service_name = \"$c$\";\n", "c", svc.name());

  ForEachMethod(svc, [&svc, printer](const MethodDescriptor& method,
                                     const std::string& input_type,
                                     const std::string& output_type) {
    printer->Print(kCppMethodDescriptor, "c", svc.name(), "i", input_type, "o",
                   output_type, "m", method.name());
  });

  printer->Print("  desc->methods.shrink_to_fit();\n");
  printer->Print("  return desc;\n");
  printer->Print("}\n\n");

  printer->Print(kCppClassDefinitions, "c", svc.name());

  ForEachMethod(svc, [&svc, printer](const MethodDescriptor& method,
                                     const std::string& input_type,
                                     const std::string& output_type) {
    printer->Print(kCppMethod, "c", svc.name(), "m", method.name(), "i",
                   input_type, "o", output_type);
  });

  for (auto it = namespaces.rbegin(); it != namespaces.rend(); it++)
    printer->Print("}  // namespace $ns$\n", "ns", *it);
}

class IPCGenerator : public ::google::protobuf::compiler::CodeGenerator {
 public:
  explicit IPCGenerator();
  ~IPCGenerator() override;

  // CodeGenerator implementation
  bool Generate(const google::protobuf::FileDescriptor* file,
                const std::string& options,
                google::protobuf::compiler::GeneratorContext* context,
                std::string* error) const override;
};

IPCGenerator::IPCGenerator() = default;
IPCGenerator::~IPCGenerator() = default;

bool IPCGenerator::Generate(const FileDescriptor* file,
                            const std::string& /*options*/,
                            GeneratorContext* context,
                            std::string* error) const {
  if (file->options().cc_generic_services()) {
    *error = "Please set \"cc_generic_service = false\".";
    return false;
  }

  const std::unique_ptr<ZeroCopyOutputStream> h_fstream(
      context->Open(GetStubName(*file) + ".h"));
  const std::unique_ptr<ZeroCopyOutputStream> cc_fstream(
      context->Open(GetStubName(*file) + ".cc"));

  // Variables are delimited by $.
  Printer h_printer(h_fstream.get(), '$');
  Printer cc_printer(cc_fstream.get(), '$');

  std::string guard = file->package() + "_" + file->name() + "_H_";
  UpperString(&guard);
  StripString(&guard, ".-/\\", '_');

  h_printer.Print(kBanner);
  h_printer.Print("#ifndef $guard$\n#define $guard$\n\n", "guard", guard);
  h_printer.Print("#include \"$h$\"\n", "h", StripName(*file) + ".pb.h");
  h_printer.Print("#include \"perfetto/ext/ipc/deferred.h\"\n");
  h_printer.Print("#include \"perfetto/ext/ipc/service.h\"\n");
  h_printer.Print("#include \"perfetto/ext/ipc/service_descriptor.h\"\n");
  h_printer.Print("#include \"perfetto/ext/ipc/service_proxy.h\"\n\n");

  cc_printer.Print(kBanner);
  cc_printer.Print("#include \"$h$\"\n", "h", GetStubName(*file) + ".h");
  cc_printer.Print("#include \"perfetto/ext/ipc/codegen_helpers.h\"\n\n");
  cc_printer.Print("#include <memory>\n");

  for (int i = 0; i < file->service_count(); i++) {
    const ServiceDescriptor* svc = file->service(i);
    GenerateServiceHeader(*file, *svc, &h_printer);
    GenerateServiceCpp(*file, *svc, &cc_printer);
  }

  h_printer.Print("#endif  // $guard$\n", "guard", guard);

  return true;
}

}  // namespace
}  // namespace ipc
}  // namespace perfetto

int main(int argc, char* argv[]) {
  ::perfetto::ipc::IPCGenerator generator;
  return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
