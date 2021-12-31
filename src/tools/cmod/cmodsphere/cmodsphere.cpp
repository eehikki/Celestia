#include <iostream>
#include <fstream>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
#include <io.h>
#endif
#include <fcntl.h>

#include <Eigen/Core>

#include <celcompat/numbers.h>


using namespace std;


// TODO: these shouldn't be hardcoded
static int latSamples = 1440;
static int longSamples = 2880;

static float* samples = nullptr;

// Read a big-endian 32-bit unsigned integer
static uint32_t readUint(istream& in)
{
    uint32_t ret;
    in.read((char*) &ret, sizeof(uint32_t));
    return (uint32_t) ret;
}


static float readFloat(istream& in)
{
    uint32_t i = readUint(in);
    uint32_t n = ((i & 0xff) << 24) | ((i & 0xff00) << 8) | ((i & 0xff0000) >> 8) | ((i & 0xff000000) >> 24);
    return *((float*) &n);
}


bool readLongLatAscii(istream& in)
{
    return false;
}


bool readBinary(istream& in,
                unsigned int latSampleCount,
                unsigned int longSampleCount)
{
    for (unsigned int i = 0; i < latSampleCount; i++)
    {
        for (unsigned int j = 0; j < longSampleCount; j++)
        {
            float r = readFloat(in) / 1000.0f;
            samples[i * longSampleCount + j] = r;
        }
    }

    return true;
}


inline float sample(float samples[],
                    unsigned int width,
                    unsigned int height,
                    float s,
                    float t)
{
    float ssamp = (float) (width - 1) + 0.99f;
    float tsamp = (float) (height - 1) + 0.99f;

    return samples[(unsigned int) (t * tsamp) * width +
                   (unsigned int) (s * ssamp)];
}


inline float sampleBilinear(const float samples[],
                            unsigned int width,
                            unsigned int height,
                            float s,
                            float t)
{
    unsigned int x0 = (unsigned int) (s * width) % width;
    unsigned int y0 = (unsigned int) (t * height) % height;
    unsigned int x1 = (unsigned int) (x0 + 1) % width;
    unsigned int y1 = (unsigned int) (y0 + 1) % height;

    float tx = s * width - (float) (unsigned int) (s * width);
    float ty = t * height - (float) (unsigned int) (t * height);

    float s00 = samples[y0 * width + x0];
    float s01 = samples[y0 * width + x1];
    float s10 = samples[y1 * width + x0];
    float s11 = samples[y1 * width + x1];

    float s0 = (1.0f - tx) * s00 + tx * s01;
    float s1 = (1.0f - tx) * s10 + tx * s11;

    return (1.0f - ty) * s0 + ty * s1;
}


// subdiv is the number of rows in the triangle
void triangleSection(unsigned int subdiv,
                     Eigen::Vector3f v0, Eigen::Vector3f v1, Eigen::Vector3f v2,
                     Eigen::Vector2f tex0, Eigen::Vector2f tex1, Eigen::Vector2f tex2)
{
    float ssamp = (float) (longSamples - 1) + 0.99f;
    float tsamp = (float) (latSamples - 1) + 0.99f;

    for (unsigned int i = 0; i <= subdiv; i++)
    {
        for (unsigned int j = 0; j <= i; j++)
        {
            float u = (i == 0) ? 0.0f : (float) j / (float) i;
            float v = (float) i / (float) subdiv;

            Eigen::Vector3f w0 = (1.0f - v) * v0 + v * v1;
            Eigen::Vector3f w1 = (1.0f - v) * v0 + v * v2;
            Eigen::Vector3f w = (1.0f - u) * w0 + u * w1;

            Eigen::Vector2f t((1.0f - u) * tex1.x() + u * tex2.x(),
                    (1.0f - v) * tex0.y() + v * tex1.y());

            w.normalize();

            if (samples != nullptr)
            {
                float theta = (float) acos(w.y());
                float phi = (float) atan2(-w.z(), w.x());
                float s = phi / (2.0f * celestia::numbers::pi_v<float>) + 0.5f;
                float t = theta / celestia::numbers::pi_v<float>;

                float r = sampleBilinear(samples, longSamples, latSamples, s, t);

                w = w * r;
            }

            cout << w.x() << " " << w.y() << " " << w.z() << " "
                 << t.x() << " " << t.y() << "\n";
        }
    }
}


// return the nth triangular number
inline unsigned int trinum(unsigned int n)
{
    return (n * (n + 1)) / 2;
}


void triangleMesh(unsigned int subdiv,
                  unsigned int baseIndex)
{
    for (unsigned int i = 0; i < subdiv; i++)
    {
        for (unsigned int j = 0; j <= i; j++)
        {
            unsigned int t0 = baseIndex + trinum(i) + j;
            unsigned int t1 = baseIndex + trinum(i + 1) + j;

            cout << t0 << " " << t1 << " " << t1 + 1 << "\n";
            if (j != i)
                cout << t0 << " " << t1 + 1 << " " << t0 + 1 << "\n";
        }
    }
}


int main(int argc, char* argv[])
{
    // Get the command line arguments
    if (argc != 4)
    {
        cerr << "Usage: cmodsphere <width> <height> <tessellation>\n";
        return 1;
    }

    if (sscanf(argv[1], "%u", &longSamples) != 1)
    {
        cerr << "Invalid width\n";
        return 1;
    }

    if (sscanf(argv[2], "%u", &latSamples) != 1)
    {
        cerr << "Invalid height\n";
        return 1;
    }

    unsigned int subdiv = 0;
    if (sscanf(argv[3], "%u", &subdiv) != 1)
    {
        cerr << "Invalid tessellation level\n";
        return 1;
    }

    samples = new float[latSamples * longSamples];

#ifdef _WIN32
    // Enable binary reads for stdin on Windows
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    // Read the height map
    readBinary(cin, latSamples, longSamples);

    // Output the mesh header
    cout << "#celmodel__ascii\n";
    cout << "\n";

    cout << "material\n";
    cout << "diffuse 0.8 0.8 0.8\n";
    cout << "end_material\n";
    cout << "\n";

    cout << "mesh\n";
    cout << "vertexdesc\n";
    cout << "position f3\n";
    cout << "texcoord0 f2\n";
    cout << "end_vertexdesc\n";
    cout << "\n";

    // Octahedral subdivision; the subdivison level for an a face
    // is one fourth the overall tessellation level.
    unsigned int primitiveFaces = 8;
    subdiv = subdiv / 4;

    unsigned int s1 = subdiv + 1;
    unsigned int verticesPerPrimFace = (s1 * s1 + s1) / 2;
    unsigned int vertexCount = primitiveFaces * verticesPerPrimFace;
    unsigned int trianglesPerPrimFace = s1 * s1 - 2 * s1 + 1;
    unsigned int triangleCount = primitiveFaces * trianglesPerPrimFace;

    cout << "vertices " << vertexCount << "\n";

    triangleSection(subdiv,
                    Eigen::Vector3f(0.0f, 1.0f, 0.0f),
                    Eigen::Vector3f(1.0f, 0.0f, 0.0f),
                    Eigen::Vector3f(0.0f, 0.0f, -1.0f),
                    Eigen::Vector2f(0.0f, 0.0f),
                    Eigen::Vector2f(0.00f, 0.5f),
                    Eigen::Vector2f(0.25f, 0.5f));
    triangleSection(subdiv,
                    Eigen::Vector3f(0.0f, 1.0f, 0.0f),
                    Eigen::Vector3f(0.0f, 0.0f, 1.0f),
                    Eigen::Vector3f(1.0f, 0.0f, 0.0f),
                    Eigen::Vector2f(0.0f, 0.0f),
                    Eigen::Vector2f(0.75f, 0.5f),
                    Eigen::Vector2f(1.00f, 0.5f));;
    triangleSection(subdiv,
                    Eigen::Vector3f(0.0f, 1.0f, 0.0f),
                    Eigen::Vector3f(-1.0f, 0.0f, 0.0f),
                    Eigen::Vector3f(0.0f, 0.0f, 1.0f),
                    Eigen::Vector2f(0.0f, 0.0f),
                    Eigen::Vector2f(0.50f, 0.5f),
                    Eigen::Vector2f(0.75f, 0.5f));
    triangleSection(subdiv,
                    Eigen::Vector3f(0.0f, 1.0f, 0.0f),
                    Eigen::Vector3f(0.0f, 0.0f, -1.0f),
                    Eigen::Vector3f(-1.0f, 0.0f, 0.0f),
                    Eigen::Vector2f(0.0f, 0.0f),
                    Eigen::Vector2f(0.25f, 0.5f),
                    Eigen::Vector2f(0.50f, 0.5f));

    triangleSection(subdiv,
                    Eigen::Vector3f(0.0f, -1.0f, 0.0f),
                    Eigen::Vector3f(0.0f, 0.0f, -1.0f),
                    Eigen::Vector3f(1.0f, 0.0f, 0.0f),
                    Eigen::Vector2f(0.0f, 1.0f),
                    Eigen::Vector2f(0.25f, 0.5f),
                    Eigen::Vector2f(0.00f, 0.5f));
    triangleSection(subdiv,
                    Eigen::Vector3f(0.0f, -1.0f, 0.0f),
                    Eigen::Vector3f(1.0f, 0.0f, 0.0f),
                    Eigen::Vector3f(0.0f, 0.0f, 1.0f),
                    Eigen::Vector2f(0.0f, 1.0f),
                    Eigen::Vector2f(1.00f, 0.5f),
                    Eigen::Vector2f(0.75f, 0.5f));
    triangleSection(subdiv,
                    Eigen::Vector3f(0.0f, -1.0f, 0.0f),
                    Eigen::Vector3f(0.0f, 0.0f, 1.0f),
                    Eigen::Vector3f(-1.0f, 0.0f, 0.0f),
                    Eigen::Vector2f(0.0f, 1.0f),
                    Eigen::Vector2f(0.75f, 0.5f),
                    Eigen::Vector2f(0.50f, 0.5f));
    triangleSection(subdiv,
                    Eigen::Vector3f(0.0f, -1.0f, 0.0f),
                    Eigen::Vector3f(-1.0f, 0.0f, 0.0f),
                    Eigen::Vector3f(0.0f, 0.0f, -1.0f),
                    Eigen::Vector2f(0.0f, 1.0f),
                    Eigen::Vector2f(0.50f, 0.5f),
                    Eigen::Vector2f(0.25f, 0.5f));

    cout << "trilist 0 " << triangleCount * 3 << "\n";

    for (unsigned int f = 0; f < primitiveFaces; f++)
    {
        triangleMesh(subdiv, f * verticesPerPrimFace);
    }

    cout << "end_mesh\n";
}
