#include "GraphicsTests.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <array>
#include <cstddef>
#include <random>
#include <string>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

std::vector<std::size_t> BackwardScan(const std::vector<const void*>& logged, const std::vector<const void*>& objects) {
    std::vector<std::size_t> indices(objects.size(), logged.size());
    for (std::size_t i = 0; i < objects.size(); ++i) {
        for (auto at = logged.size(); at-- > 0;) {
            if (logged[at] == objects[i]) {
                indices[i] = at;
                break;
            }
        }
    }
    return indices;
}

void CheckIndices(const std::vector<const void*>& logged, const std::vector<const void*>& objects, const std::string& what) {
    std::vector<std::size_t> indices;
    LastRecordIndices(logged, objects, indices);
    Require(indices == BackwardScan(logged, objects), "last record indices, " + what + ": an object did not get the index of its last record");
    std::vector<std::size_t> sorted;
    LastRecordIndicesSorted(logged, objects, sorted);
    Require(indices == sorted, "last record indices, " + what + ": the hashed indices differ from the sorted ones");
}

}

void RunLastRecordIndicesTests() {
    std::array<int, 4096> pool{};
    const auto object = [&pool](std::size_t index) -> const void* { return &pool[index]; };

    CheckIndices({object(0), object(1), object(0), object(2)}, {object(0), object(1), object(2), object(3), object(0), nullptr}, "repeated records and objects");
    CheckIndices({}, {object(0), nullptr}, "an empty log");
    CheckIndices({object(0), nullptr, object(1)}, {}, "no objects");
    CheckIndices({nullptr, object(0)}, {nullptr, object(0)}, "a null object");

    std::vector<const void*> table;
    for (std::size_t texture = 0; texture < 1500; ++texture) table.push_back(object(texture));
    CheckIndices(table, table, "a table of 1500 textures over its own records in table order");
    std::vector<const void*> repeated = table;
    repeated.insert(repeated.end(), table.begin(), table.begin() + 548);
    CheckIndices(repeated, table, "a table of 1500 textures whose first 548 were looked up twice");
    std::vector<const void*> missing(table.begin() + 476, table.end());
    CheckIndices(missing, table, "a table of 1500 textures over a log without its first 476 records");

    std::mt19937 generator(0x5eed);
    for (std::size_t round = 0; round < 40; ++round) {
        std::vector<const void*> logged(generator() % 200);
        for (auto& entry : logged) entry = generator() % 6 == 0 ? nullptr : object(generator() % 64);
        std::vector<const void*> objects;
        for (std::size_t source = generator() % 64; source-- > 0;) {
            const auto repeats = 1 + generator() % 50;
            const void* chosen = generator() % 8 == 0 ? nullptr : object(generator() % 96);
            for (std::size_t copy = 0; copy < repeats; ++copy) objects.push_back(chosen);
        }
        CheckIndices(logged, objects, "random round " + std::to_string(round) + " with nulls, absent objects and objects repeated up to 50 times");
    }
    for (const std::size_t records : {std::size_t{1}, std::size_t{7}, std::size_t{64}, std::size_t{1023}, std::size_t{1024}, std::size_t{1500}, std::size_t{2048}}) {
        for (const std::size_t textures : {std::size_t{1}, std::size_t{5}, std::size_t{300}, std::size_t{1500}}) {
            for (const std::size_t distinct : {std::size_t{8}, pool.size()}) {
                std::vector<const void*> logged(records);
                for (auto& entry : logged) entry = object(generator() % distinct);
                std::vector<const void*> objects(textures);
                for (auto& entry : objects) entry = object(generator() % distinct);
                CheckIndices(logged, objects, std::to_string(records) + " records, " + std::to_string(textures) + " objects of " + std::to_string(distinct));
            }
        }
    }
}
