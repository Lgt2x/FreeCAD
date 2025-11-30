#include "SMDS_Mesh.hxx"
#include "SMDS_UnstructuredGrid.hxx"
#include <vtkCellType.h>
#include <vtkCubeSource.h>
#include <vtkElevationFilter.h>
#include <vtkPoints.h>
#include <vtkSetGet.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridWriter.h>

int main(int argc, char* argv[])
{
    SMDS_Mesh mesh;
    mesh.AddNode(0.0, 0.0, 0.0);
    mesh.AddNode(2.0, 0.0, 0.0);
    mesh.AddNode(1.0, 2.0, 0.0);
    mesh.AddNode(1.0, 0.0, 2.0);
    mesh.AddNode(2.0, 2.0, 2.0);
    mesh.AddNode(4.0, 4.0, 4.0);  // Unused point
    mesh.AddNode(0.0, 2.0, 3.0);

    std::vector<int> faceNodeCount {3, 3, 3, 3};
    std::vector<int> conn1 {1, 2, 3, 1, 2, 4, 1, 3, 4, 2, 3, 4};
    std::vector<int> conn2 {2, 3, 4, 2, 3, 5, 2, 4, 5, 3, 4, 5};
    std::vector<int> conn3 {3, 4, 5, 3, 5, 7, 4, 5, 7, 3, 4, 7};
    mesh.AddPolyhedralVolumeWithID(conn1, faceNodeCount, 0);
    mesh.AddPolyhedralVolumeWithID(conn2, faceNodeCount, 1);
    mesh.AddPolyhedralVolumeWithID(conn3, faceNodeCount, 2);


    vtkNew<vtkXMLUnstructuredGridWriter> writer;
    writer->SetInputData(mesh.getGrid());
    writer->SetDataModeToAscii();
    writer->SetFileName("/tmp/ug.vtu");
    writer->Write();

    std::vector<int> idNodesOldToNew {1, 2, 3, 4, 5, -1, 7};
    std::vector<int> idCellsOldToNew(3);
    auto grid = mesh.getGrid();
    grid->compactGrid(idNodesOldToNew, 6, idCellsOldToNew, 3);

    writer->SetInputData(mesh.getGrid());
    writer->SetFileName("/tmp/ug_compact.vtu");
    writer->Write();
}
