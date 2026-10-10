#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iomanip>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <vector>
namespace tpc::analytics::output
{
    class VtkFieldExporter
    {
    public:
        static std::expected<void, std::string>
        export_3d_field_to_vtk(std::span<const double> field_data,
                               std::span<const double> coordinates_data,
                               std::string_view file_path)
        {
            constexpr std::size_t dimension = 3;

            if (coordinates_data.empty())
                return std::unexpected("Coordinates data is empty");

            if (coordinates_data.size() % dimension != 0)
                return std::unexpected("Coordinates data size must be divisible by 3");

            const std::size_t num_points =
                coordinates_data.size() / dimension;

            if (field_data.size() != num_points * dimension)
            {
                return std::unexpected(std::format(
                    "Field data must contain {} values, but contains {}",
                    num_points * dimension,
                    field_data.size()));
            }

            if (file_path.empty())
                return std::unexpected("File path is empty");

            struct CartesianPoint
            {
                double x;
                double y;
                double z;
            };

            std::vector<CartesianPoint> points;
            points.reserve(num_points);

            // The analytics grid is stored as r, phi, z. VTK geometry and
            // topology are expressed in Cartesian coordinates.
            for (std::size_t offset = 0;
                 offset < coordinates_data.size();
                 offset += dimension)
            {
                const double r = coordinates_data[offset];
                const double phi = coordinates_data[offset + 1];

                points.push_back({
                    .x = r * std::cos(phi),
                    .y = r * std::sin(phi),
                    .z = coordinates_data[offset + 2]
                });
            }

            const auto approximately_equal = [](double lhs, double rhs) {
                const double scale = std::max({1.0, std::abs(lhs), std::abs(rhs)});
                return std::abs(lhs - rhs) <= 64.0 * std::numeric_limits<double>::epsilon() * scale;
            };

            // Points are generated one complete XY layer at a time.
            std::size_t points_per_layer = 1;
            while (points_per_layer < num_points
                   && approximately_equal(points[points_per_layer].z, points.front().z))
            {
                ++points_per_layer;
            }

            if (points_per_layer == num_points || num_points % points_per_layer != 0)
                return std::unexpected("Coordinates do not form complete Z layers");

            const std::size_t layer_count = num_points / points_per_layer;
            if (layer_count < 2)
                return std::unexpected("At least two Z layers are required for volumetric export");

            for (std::size_t layer = 0; layer < layer_count; ++layer)
            {
                const std::size_t layer_offset = layer * points_per_layer;
                const double layer_z = points[layer_offset].z;

                for (std::size_t point = 0; point < points_per_layer; ++point)
                {
                    const auto& reference = points[point];
                    const auto& current = points[layer_offset + point];

                    if (!approximately_equal(current.z, layer_z)
                        || !approximately_equal(current.x, reference.x)
                        || !approximately_equal(current.y, reference.y))
                    {
                        return std::unexpected("XY topology differs between Z layers");
                    }
                }
            }

            struct Row
            {
                std::size_t begin;
                std::size_t end;
            };

            std::vector<Row> rows;
            for (std::size_t begin = 0; begin < points_per_layer;)
            {
                std::size_t end = begin + 1;
                while (end < points_per_layer
                       && approximately_equal(points[end].y, points[begin].y))
                {
                    ++end;
                }

                rows.push_back({begin, end});
                begin = end;
            }

            if (rows.size() < 2)
                return std::unexpected("At least two XY rows are required for volumetric export");

            std::vector<std::array<std::size_t, 3>> triangles;
            triangles.reserve(points_per_layer * 2);

            const auto add_triangle = [&](std::size_t a, std::size_t b, std::size_t c) {
                const double signed_area =
                    (points[b].x - points[a].x) * (points[c].y - points[a].y)
                    - (points[b].y - points[a].y) * (points[c].x - points[a].x);

                if (approximately_equal(signed_area, 0.0))
                    return;

                if (signed_area < 0.0)
                    std::swap(b, c);

                triangles.push_back({a, b, c});
            };

            // Triangulate each strip between adjacent staggered rows. Extruding
            // these triangles through Z produces wedge cells filling the cylinder.
            for (std::size_t row = 0; row + 1 < rows.size(); ++row)
            {
                const Row lower = rows[row];
                const Row upper = rows[row + 1];
                std::size_t lower_point = lower.begin;
                std::size_t upper_point = upper.begin;

                while (lower_point + 1 < lower.end || upper_point + 1 < upper.end)
                {
                    if (upper_point + 1 == upper.end)
                    {
                        add_triangle(lower_point, lower_point + 1, upper_point);
                        ++lower_point;
                    }
                    else if (lower_point + 1 == lower.end)
                    {
                        add_triangle(lower_point, upper_point, upper_point + 1);
                        ++upper_point;
                    }
                    else if (points[lower_point + 1].x < points[upper_point + 1].x)
                    {
                        add_triangle(lower_point, lower_point + 1, upper_point);
                        ++lower_point;
                    }
                    else
                    {
                        add_triangle(lower_point, upper_point, upper_point + 1);
                        ++upper_point;
                    }
                }
            }

            if (triangles.empty())
                return std::unexpected("Failed to build XY triangulation");

            if (triangles.size() > std::numeric_limits<std::size_t>::max() / (layer_count - 1))
                return std::unexpected("VTK cell collection is too large");

            const std::size_t cell_count = triangles.size() * (layer_count - 1);
            if (cell_count > std::numeric_limits<std::size_t>::max() / 7)
                return std::unexpected("VTK cell collection is too large");

            const std::filesystem::path path{file_path};

            if (path.has_parent_path())
            {
                std::error_code ec;
                std::filesystem::create_directories(path.parent_path(), ec);

                if (ec)
                    return std::unexpected(
                        std::format("Failed to create directory: {}", ec.message()));
            }

            std::ofstream output(path);
            if (!output.is_open())
                return std::unexpected(
                    std::format("Failed to open file for writing: {}", path.string()));

            output << std::scientific << std::setprecision(10);

            output << "# vtk DataFile Version 3.0\n";
            output << "TPC Analytics Field Data; coordinates in mm; field in G\n";
            output << "ASCII\n";
            output << "DATASET UNSTRUCTURED_GRID\n";

            output << "POINTS " << num_points << " double\n";

            for (const auto& point : points)
                output << point.x << ' ' << point.y << ' ' << point.z << '\n';

            output << "\nCELLS " << cell_count << ' ' << cell_count * 7 << '\n';

            for (std::size_t layer = 0; layer + 1 < layer_count; ++layer)
            {
                const std::size_t lower_offset = layer * points_per_layer;
                const std::size_t upper_offset = lower_offset + points_per_layer;

                for (const auto& triangle : triangles)
                {
                    output << "6 "
                           << lower_offset + triangle[0] << ' '
                           << lower_offset + triangle[1] << ' '
                           << lower_offset + triangle[2] << ' '
                           << upper_offset + triangle[0] << ' '
                           << upper_offset + triangle[1] << ' '
                           << upper_offset + triangle[2] << '\n';
                }
            }

            // VTK_WEDGE = 13.
            output << "\nCELL_TYPES " << cell_count << '\n';
            for (std::size_t cell = 0; cell < cell_count; ++cell)
                output << "13\n";

            output << "\nPOINT_DATA " << num_points << '\n';
            output << "VECTORS MagneticField double\n";

            // Br, Bphi, Bz -> Bx, By, Bz
            for (std::size_t offset = 0;
                 offset < field_data.size();
                 offset += dimension)
            {
                const double phi  = coordinates_data[offset + 1];
                const double br   = field_data[offset];
                const double bphi = field_data[offset + 1];
                const double bz   = field_data[offset + 2];

                const double bx =
                    br * std::cos(phi) - bphi * std::sin(phi);

                const double by =
                    br * std::sin(phi) + bphi * std::cos(phi);

                output << bx << ' ' << by << ' ' << bz << '\n';
            }

            output.flush();

            if (!output)
                return std::unexpected("I/O error while writing VTK file");

            return {};
        }
    };
}
