#include <iostream>

#include <vtkm/Math.h>
#include <vtkm/cont/ArrayHandle.h>
//#include <vtkm/worklet/WorkletMapField.h>

// use VTK-m master branch

// create one array handle containing Vec3s
// create a simple worklet that computes the sum of an input Vec3
// tile a loop over the array handle
// outer loop:
//   create an intermediate stack-allocated array that is 3x the block size
//   store Vec3 elements into the array offset by block size elements
//   create an intermediate output array similarly, but leave it empty
// inner loop:
//   create a new Vec3 by pulling from the stack-allocated array
//   pass it to the worklet and run the worklet
//   store the output using the VecTraits class to mask the scalar as a Vec1
// see if it vectorizes!!

// worklet
class VectorSum //: public vtkm::worklet::WorkletMapField
{
  public:
    /*
    typedef void ControlSignature(FieldIn<Vec3> input,
                                  FieldOut<Scalar> output);
    typedef _2 ExecutionSignature(_1);
    typedef _1 InputDomain;
    */

    VTKM_CONT
    VectorSum() {};

    VTKM_EXEC
    vtkm::Float32 operator()(const vtkm::Vec<vtkm::Float32,3>& input) const
    {
      return input[0] + input[1] + input[2];
    }
};

// single worklet call
vtkm::Float32 singleWorkletCall(const vtkm::Vec<vtkm::Float32,3> &input)
{
  // call the VectorSum worklet
  // return the output
  VectorSum worklet;
  return worklet(input);
}

// inner loop
template<vtkm::Id TILE>
void innerLoop(
    const vtkm::Float32 (&compArray)[3*TILE],
    vtkm::Float32 (&outputArray)[TILE],
    const vtkm::Id &bound = TILE)
{
  // for TILE
  // create a Vec3 from the component array
  // pass it to a single worklet call
  // save the output to the output array
  for(vtkm::Id iteration = 0; iteration < bound; iteration++)
  {
    vtkm::Vec<vtkm::Float32,3> inputValue(
        compArray[0*TILE+iteration],
        compArray[1*TILE+iteration],
        compArray[2*TILE+iteration]);
    outputArray[iteration] = singleWorkletCall(inputValue);
  }
}

// outer loop
template<vtkm::Id TILE>
vtkm::cont::ArrayHandle<vtkm::Float32> outerLoop(
    const vtkm::cont::ArrayHandle<vtkm::Vec<vtkm::Float32,3> > &inputData)
{
  // setup code, not part of tiling
  vtkm::cont::ArrayHandle<vtkm::Vec<vtkm::Float32,3> >::PortalConstControl
    inputPortal = inputData.GetPortalConstControl();
  vtkm::cont::ArrayHandle<vtkm::Float32> result;
  result.Allocate(inputPortal.GetNumberOfValues());
  vtkm::cont::ArrayHandle<vtkm::Float32>::PortalControl
    resultPortal = result.GetPortalControl();
  // end setup code

  const vtkm::Id numTiles = inputPortal.GetNumberOfValues() / TILE;
  const vtkm::Id remainder = inputPortal.GetNumberOfValues() % TILE;
  // the real outer loop
  for(vtkm::Id tileIndex = 0; tileIndex < numTiles; tileIndex++)
  {
    vtkm::Id tileOffset = tileIndex*TILE;
    // create intermediate input array
    // AoS -> SoA
    vtkm::Float32 tileInputCompArray[3*TILE];
    for(vtkm::Id element = 0; element < TILE; element++)
    {
      vtkm::Vec<vtkm::Float32,3> value = inputPortal.Get(tileOffset + element);
      // this would be a loop if vec size was unknown
      tileInputCompArray[0*TILE+element] = value[0];
      tileInputCompArray[1*TILE+element] = value[1];
      tileInputCompArray[2*TILE+element] = value[2];
    }
    // create empty intermediate output array
    vtkm::Float32 tileOutputArray[TILE];

    // call inner loop
    // pass output AoS by reference to inner loop
    innerLoop<TILE>(tileInputCompArray, tileOutputArray);
    
    // copy back to output array handle
    for(vtkm::Id element = 0; element < TILE; element++)
    {
      resultPortal.Set(tileOffset + element, tileOutputArray[element]);
    }
  }
  // done with full-size tiles

  // do remainder
  // create another set of intermediate arrays
  vtkm::Id offset = numTiles * TILE;
  vtkm::Float32 remInputCompArray[3*TILE];
  for(vtkm::Id element = 0; element < remainder; element++)
  {
    vtkm::Vec<vtkm::Float32,3> value = inputPortal.Get(offset + element);
    remInputCompArray[0*TILE+element] = value[0];
    remInputCompArray[1*TILE+element] = value[1];
    remInputCompArray[2*TILE+element] = value[2];
  }
  vtkm::Float32 remOutputArray[TILE];

  innerLoop<TILE>(remInputCompArray, remOutputArray, remainder);

  for(vtkm::Id element = 0; element < remainder; element++)
  {
    resultPortal.Set(offset + element, remOutputArray[element]);
  }
  // done with remainder

  return result;
}

// check
bool checkResult(vtkm::cont::ArrayHandle<vtkm::Float32> result)
{
  // loop over the array handle
  // each value should be 3x its index
  vtkm::Id numErrors = 0;
  vtkm::cont::ArrayHandle<vtkm::Float32>::PortalConstControl
    resultPortal = result.GetPortalConstControl();
  for(vtkm::Id index = 0; index < resultPortal.GetNumberOfValues(); index++)
  {
    vtkm::Float32 value = resultPortal.Get(index);
    if(value != 3*index)
    {
      if(numErrors < 5)
      {
        std::cerr << "ERROR: ";
        std::cerr << "result[" << index << "] = ";
        std::cerr << value << ", but should be ";
        std::cerr << 3*index << std::endl;
      }
      numErrors++;
    }
  }
  if(numErrors > 0)
  {
    std::cerr << "Total errors: " << numErrors << std::endl;
  }
}

int main(int argc, char **argv)
{
  // this cannot be larger than 2^23 - 1
  // since we're using Float32s
  vtkm::Id LENGTH = 10000000;
  const vtkm::Id TILESIZE = 1024;

  vtkm::cont::ArrayHandle<vtkm::Vec<vtkm::Float32,3> > data;
  data.Allocate(LENGTH);

  // create the data
  vtkm::cont::ArrayHandle<vtkm::Vec<vtkm::Float32,3> >::PortalControl
    dataPortal = data.GetPortalControl();
  for(vtkm::Id index = 0; index < LENGTH; index++)
  {
    dataPortal.Set(index, vtkm::Vec<vtkm::Float32,3>(index, index, index));
  }

  // run outer loop
  vtkm::cont::ArrayHandle<vtkm::Float32>
    result = outerLoop<TILESIZE>(data);

  // check results
  bool ok = checkResult(result);

  return 0;
}
