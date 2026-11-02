#include "ModuleLifter.hpp"

#include <lifter/Hex.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace Ps5Lift {

namespace Lifting {

namespace {

class TableLifter {
public:
    TableLifter(llvm::LLVMContext& context, const GuestProgram& program, const std::vector<std::map<std::uint64_t, GuestFunction>>& functions, const LiftOptions& options, const Target& target)
        : _context(context), _program(program), _functions(functions), _options(options), _module(std::make_unique<llvm::Module>("ps5lift", context)),
          _pointer(llvm::PointerType::get(context, 0)), _i64(llvm::Type::getInt64Ty(context)) {
        target.Configure(*_module);
    }

    std::unique_ptr<llvm::Module> Lift() {
        auto* i32 = llvm::Type::getInt32Ty(_context);
        _segmentType = llvm::StructType::create(_context, {_i64, _i64, _i64, _pointer, i32}, "Ps5LiftSegment");
        _dataImportType = llvm::StructType::create(_context, {_i64, _pointer, _i64}, "Ps5LiftDataImport");
        _importType = llvm::StructType::create(_context, {_i64, _pointer}, "Ps5LiftImport");
        _exportType = llvm::StructType::create(_context, {_pointer, _i64}, "Ps5LiftExport");
        _imageType = llvm::StructType::create(_context, {_pointer, _i64, _i64, _i64, _pointer, _i64, _i64, _i64, _i64, _i64, _i64, _i64, _pointer, _i64, _pointer, _i64, _i64, _i64, _i64, _pointer, _i64},
                                              "Ps5LiftImage");
        auto* functionType = llvm::StructType::create(_context, {_i64, _pointer}, "Ps5LiftFunctionEntry");
        auto* programType = llvm::StructType::create(_context, {_pointer, _i64, _pointer, _i64, _pointer, _i64, _i64}, "Ps5LiftProgram");

        std::vector<llvm::Constant*> images;
        for (std::size_t index = 0; index < _program.Images.size(); ++index) images.push_back(Image(index));
        std::map<std::uint64_t, llvm::Constant*> sorted;
        for (const auto& functions : _functions) {
            for (const auto& [address, function] : functions) sorted.emplace(address, DeclareGuest(*_module, address));
        }
        std::vector<llvm::Constant*> entries;
        for (const auto& [address, function] : sorted) entries.push_back(llvm::ConstantStruct::get(functionType, {Int(address), function}));
        std::vector<llvm::Constant*> initialization;
        for (const auto index : _program.Initialization) initialization.push_back(Int(index));

        auto* program = llvm::ConstantStruct::get(programType, {Table(_imageType, images, "ps5lift_images"), Int(images.size()), Table(_i64, initialization, "ps5lift_initialization"), Int(initialization.size()),
            Table(functionType, entries, "ps5lift_functions"), Int(entries.size()), Int(_program.Images.front().Entry())});
        new llvm::GlobalVariable(*_module, programType, true, llvm::GlobalValue::ExternalLinkage, program, "ps5lift_program");
        return Verified(std::move(_module));
    }

private:
    llvm::LLVMContext& _context;
    const GuestProgram& _program;
    const std::vector<std::map<std::uint64_t, GuestFunction>>& _functions;
    const LiftOptions& _options;
    std::unique_ptr<llvm::Module> _module;
    llvm::PointerType* _pointer;
    llvm::IntegerType* _i64;
    llvm::StructType* _segmentType = nullptr;
    llvm::StructType* _dataImportType = nullptr;
    llvm::StructType* _importType = nullptr;
    llvm::StructType* _exportType = nullptr;
    llvm::StructType* _imageType = nullptr;
    std::map<std::string, llvm::GlobalVariable*> _dataSymbols;

    llvm::Constant* Int(const std::uint64_t value) { return llvm::ConstantInt::get(_i64, value); }

    llvm::Constant* Table(llvm::Type* type, const std::vector<llvm::Constant*>& values, const char* name) {
        auto* array = llvm::ConstantArray::get(llvm::ArrayType::get(type, values.size()), values);
        return new llvm::GlobalVariable(*_module, array->getType(), true, llvm::GlobalValue::PrivateLinkage, array, name);
    }

    llvm::Constant* Image(const std::size_t index) {
        const auto& image = _program.Images[index];
        auto* i32 = llvm::Type::getInt32Ty(_context);
        std::vector<llvm::Constant*> segments;
        for (const auto& segment : image.Segments()) {
            auto* data = llvm::ConstantDataArray::get(_context, llvm::ArrayRef<std::uint8_t>(segment.Bytes));
            auto* global = new llvm::GlobalVariable(*_module, data->getType(), true, llvm::GlobalValue::PrivateLinkage, data, "segment_" + Hex(segment.Address));
            global->setAlignment(llvm::Align(16));
            segments.push_back(llvm::ConstantStruct::get(_segmentType, {Int(segment.Address), Int(segment.MemorySize), Int(segment.Bytes.size()), global, llvm::ConstantInt::get(i32, segment.Flags)}));
        }
        std::vector<llvm::Constant*> dataImports;
        for (const auto& import : image.DataImports()) {
            const auto symbol = "ps5lift_data_" + BridgeSymbol(ImportName(_options, import.Name)).substr(std::string_view("ps5lift_import_").size());
            auto& global = _dataSymbols[symbol];
            if (!global) global = new llvm::GlobalVariable(*_module, _pointer, true, llvm::GlobalValue::ExternalLinkage, nullptr, symbol);
            dataImports.push_back(llvm::ConstantStruct::get(_dataImportType, {Int(import.Slot), global, Int(static_cast<std::uint64_t>(import.Addend))}));
        }
        std::vector<llvm::Constant*> imports;
        for (const auto& [address, name] : image.Imports()) {
            auto* text = llvm::IRBuilder<>(_context).CreateGlobalString(ImportName(_options, name), "import_name", 0, _module.get());
            imports.push_back(llvm::ConstantStruct::get(_importType, {Int(address), text}));
        }
        std::vector<llvm::Constant*> exports;
        for (const auto& [symbol, address] : image.Exports()) {
            auto* text = llvm::IRBuilder<>(_context).CreateGlobalString(symbol, "export_name", 0, _module.get());
            exports.push_back(llvm::ConstantStruct::get(_exportType, {text, Int(address)}));
        }
        auto* name = llvm::IRBuilder<>(_context).CreateGlobalString(_program.LoaderName(index), "image_name", 0, _module.get());
        const auto& tls = image.Tls();
        const auto& placement = image.TlsPlacement();
        return llvm::ConstantStruct::get(_imageType, {name, Int(image.LoadBias()), Int(image.ProgramHeaders().Address), Int(image.ProgramHeaders().Count), Table(_segmentType, segments, "ps5lift_segments"), Int(segments.size()),
            Int(placement.Module), Int(tls.Address), Int(tls.FileSize), Int(tls.MemorySize), Int(tls.Alignment), Int(placement.Offset), Table(_dataImportType, dataImports, "ps5lift_data_imports"), Int(dataImports.size()),
            Table(_importType, imports, "ps5lift_imports"), Int(imports.size()), Int(image.Init()), Int(image.InitArray().Address), Int(image.InitArray().Count),
            Table(_exportType, exports, "ps5lift_exports"), Int(exports.size())});
    }
};

}

}

std::unique_ptr<llvm::Module> LiftTables(llvm::LLVMContext& context, const GuestProgram& program, const std::vector<std::map<std::uint64_t, GuestFunction>>& functions, const LiftOptions& options,
                                         const Target& target) {
    return Lifting::TableLifter(context, program, functions, options, target).Lift();
}

}
