#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include <stdlib.h>
#include <string.h>

using Id = unsigned long long;
using Float32 = float;

Id numIterations = 1;
Id SIDE_LENGTH = 256;
Id numVectors = SIDE_LENGTH*SIDE_LENGTH*SIDE_LENGTH;
bool saveTimes = false;
std::string fileSuffix;

class Vec3
{
  public:
    Vec3()
    {
      components[0] = 0;
      components[1] = 0;
      components[2] = 0;
    }
    Vec3(Float32 x, Float32 y, Float32 z)
    {
      components[0] = x;
      components[1] = y;
      components[2] = z;
    }

    Float32& operator[](unsigned int index)
    {
      return components[index];
    }

    Float32 operator[](unsigned int index) const
    {
      return components[index];
    }

  private:
    Float32 components[3];
};

Float32 dotProductBasic(const Vec3 &v1, const Vec3 &v2)
{
  return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void computeDotProducts(const Vec3 *vec1, const Vec3 *vec2, Float32 *out)
{
  Id total = 0;
  Id times[numIterations];
  std::cout << "Runs Single Average" << std::endl;
  for(Id index = 0; index < numIterations; index++)
  {
    if(index > 0)
      std::cout << "\r";
    auto s = std::chrono::high_resolution_clock::now();

    for(Id vecIndex = 0; vecIndex < numVectors; vecIndex++)
    {
      out[vecIndex] = dotProductBasic(vec1[vecIndex], vec2[vecIndex]);
    }

    auto e = std::chrono::high_resolution_clock::now();
    Id run =
      std::chrono::duration_cast<std::chrono::microseconds>(e-s).count();
    total += run;
    times[index] = run;
    std::cout << std::setw(4) << index+1 << " ";
    std::cout << std::setw(6) << run/1000.0 << " ";
    std::cout << std::setw(7) << total / (index+1) / 1000.0;
    std::cout << " ms   " << std::flush;
  }
  std::cout << std::endl;

  if(saveTimes)
  {
    std::string filename = "times_" + std::to_string(SIDE_LENGTH) +
      "_" + fileSuffix + ".csv";
    std::ofstream outfile(filename);
    for(Id index = 0; index < numIterations; index++)
    {
      outfile << times[index] << '\n';
    }
  }
}

void checkResults(const Float32 *results)
{
  Id numErrors = 0;
  Float32 floatError = 0.000001;
  for(Id index = 0; index < numVectors; index++)
  {
    const Float32 value =
      3*(float)index/numVectors*(float)index/numVectors;
    if(value > results[index] + floatError ||
       value < results[index] - floatError)
    {
      numErrors++;
      if(numErrors < 5)
      {
        std::cerr << "ERROR: results[" << index << "] = " << results[index];
        std::cerr << ", but should be " << value << std::endl;
      }
    }
  }
  if(numErrors > 0)
  {
    std::cerr << "Total errors: " << numErrors << std::endl;
  }
}

int main(int argc, char **argv)
{
  for(int argi = 1; argi < argc; argi++)
  {
    if(strcmp(argv[argi], "-n") == 0)
    {
      if(argi + 1 >= argc) {
        std::cerr << "no number provided to -n" << std::endl;
        return 1;
      }
      numIterations = atoi(argv[++argi]);
    }
    if(strcmp(argv[argi], "-l") == 0)
    {
      if(argi + 1 >= argc) {
        std::cerr << "no number provided to -l" << std::endl;
        return 1;
      }
      SIDE_LENGTH = atoi(argv[++argi]);
    }
    if(strcmp(argv[argi], "-s") == 0)
    {
      if(argi + 1 >= argc) {
        std::cerr << "no name provided to -s" << std::endl;
        return 1;
      }
      fileSuffix = argv[++argi];
      saveTimes = true;
    }
  }
 
  numVectors = SIDE_LENGTH*SIDE_LENGTH*SIDE_LENGTH;
  Vec3 *vectors1 = new Vec3[numVectors];
  Vec3 *vectors2 = new Vec3[numVectors];
  Float32 *dotProductsBasic = new Float32[numVectors];

  // fill in the input arrays
  for(Id index = 0; index < numVectors; index++)
  {
    Float32 value = (float)index / numVectors;
    vectors1[index] = {value, value, value};
    vectors2[index] = {value, value, value};
  }

  computeDotProducts(vectors1, vectors2, dotProductsBasic);
  checkResults(dotProductsBasic);

  return 0;
}
