/*

Copyright (C) University of Oxford, 2005-2011

University of Oxford means the Chancellor, Masters and Scholars of the
University of Oxford, having an administrative office at Wellington
Square, Oxford OX1 2JD, UK.

This file is part of Chaste.

Chaste is free software: you can redistribute it and/or modify it
under the terms of the GNU Lesser General Public License as published
by the Free Software Foundation, either version 2.1 of the License, or
(at your option) any later version.

Chaste is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
License for more details. The offer of Chaste under the terms of the
License is subject to the License being interpreted in accordance with
English Law and subject to any action against the University of Oxford
being under the jurisdiction of the English Courts.

You should have received a copy of the GNU Lesser General Public License
along with Chaste. If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef TESTCABASEDIMULATIONWITHCABASEDCELLPOPULATION_HPP_
#define TESTCABASEDIMULATIONWITHCABASEDCELLPOPULATION_HPP_

#include <cxxtest/TestSuite.h>

// Must be included before other cell_based headers
#include "CellBasedSimulationArchiver.hpp"

#include "CellsGenerator.hpp"
#include "CaBasedSimulation.hpp"
#include "FixedDurationGenerationBasedCellCycleModel.hpp"
#include "CaBasedCellPopulation.hpp"
#include "DiffusionCaUpdateRule.hpp"
#include "AdvectionCaUpdateRule.hpp"
#include "AbstractCellBasedTestSuite.hpp"
#include "WildTypeCellMutationState.hpp"
#include "RandomCellKiller.hpp"
#include "NumericFileComparison.hpp"
#include "CellBasedEventHandler.hpp"
#include "SmartPointers.hpp"

class TestCaBasedSimulationWithCaBasedCellPopulation : public AbstractCellBasedTestSuite
{
private:

    /**
     * Compare two meshes to see if they are 'the same'.  Doesn't check everything,
     * but is fairly thorough. Used for testing serialization.
     */
    template<unsigned DIM>
    void CompareMeshes(TetrahedralMesh<DIM,DIM>* pMesh1,
                       TetrahedralMesh<DIM,DIM>* pMesh2)
    {
        TS_ASSERT_EQUALS(pMesh1->GetNumAllNodes(), pMesh2->GetNumAllNodes());
        TS_ASSERT_EQUALS(pMesh1->GetNumNodes(), pMesh2->GetNumNodes());
        TS_ASSERT_EQUALS(pMesh1->GetNumBoundaryNodes(), pMesh2->GetNumBoundaryNodes());

        for (unsigned i=0; i<pMesh1->GetNumAllNodes(); i++)
        {
            Node<DIM>* p_node = pMesh1->GetNode(i);
            Node<DIM>* p_node2 = pMesh2->GetNode(i);
            TS_ASSERT_EQUALS(p_node->IsDeleted(), p_node2->IsDeleted());
            TS_ASSERT_EQUALS(p_node->GetIndex(), p_node2->GetIndex());
            TS_ASSERT_EQUALS(p_node->IsBoundaryNode(), p_node2->IsBoundaryNode());
            for (unsigned j=0; j<DIM; j++)
            {
                TS_ASSERT_DELTA(p_node->rGetLocation()[j], p_node2->rGetLocation()[j], 1e-16);
            }
        }

        TS_ASSERT_EQUALS(pMesh1->GetNumElements(), pMesh2->GetNumElements());
        TS_ASSERT_EQUALS(pMesh1->GetNumAllElements(), pMesh2->GetNumAllElements());
        TS_ASSERT_EQUALS(pMesh1->GetNumBoundaryElements(), pMesh2->GetNumBoundaryElements());
        TS_ASSERT_EQUALS(pMesh1->GetNumAllBoundaryElements(), pMesh2->GetNumAllBoundaryElements());

        typename AbstractTetrahedralMesh<DIM,DIM>::ElementIterator iter2 = pMesh2->GetElementIteratorBegin();
        for (typename AbstractTetrahedralMesh<DIM,DIM>::ElementIterator iter = pMesh1->GetElementIteratorBegin();
             iter != pMesh1->GetElementIteratorEnd();
             ++iter, ++iter2)
        {
            TS_ASSERT_EQUALS(iter->GetNumNodes(), iter2->GetNumNodes());
            for (unsigned i=0; i<iter->GetNumNodes(); i++)
            {
                TS_ASSERT_EQUALS(iter->GetNodeGlobalIndex(i), iter2->GetNodeGlobalIndex(i));
            }
        }
    }

public:

    void TestCellsDiffusing() throw (Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(10, 10, true); // 11*11 nodes

        // Create two cells
        MAKE_PTR(WildTypeCellMutationState, p_state);

        FixedDurationGenerationBasedCellCycleModel* p_model_1 = new FixedDurationGenerationBasedCellCycleModel();
        p_model_1->SetCellProliferativeType(DIFFERENTIATED);
        CellPtr p_cell_1(new Cell(p_state, p_model_1));

        FixedDurationGenerationBasedCellCycleModel* p_model_2 = new FixedDurationGenerationBasedCellCycleModel();
        p_model_2->SetCellProliferativeType(DIFFERENTIATED);
        CellPtr p_cell_2(new Cell(p_state, p_model_2));

        std::vector<CellPtr> cells;
        cells.push_back(p_cell_1);
        cells.push_back(p_cell_2);

        std::vector<unsigned> real_node_indices;
        real_node_indices.push_back(47);
        real_node_indices.push_back(73);

        // Create a cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetDt(1);
        simulator.SetEndTime(20);

        // Pass an update rule to the simulation
        MAKE_PTR(DiffusionCaUpdateRule<2>, p_update_rule);
        simulator.AddUpdateRule(p_update_rule);

        TS_ASSERT_THROWS_THIS(simulator.Solve(), "OutputDirectory not set");
        CellBasedEventHandler::Reset();

        // Destroy the simulation time class because of failed solve
        SimulationTime::Destroy();
        SimulationTime::Instance()->SetStartTime(0.0);

        simulator.SetOutputDirectory("TestCellsDiffusing");

        // Run simulation
        simulator.Solve();

        TS_ASSERT_EQUALS(cell_population.GetNumRealCells(), 2u);

        // Coverage of CalculateCellDivisionVector()
        c_vector<double, 2> cell_division_vector = simulator.CalculateCellDivisionVector(*(cell_population.Begin()));
        TS_ASSERT_DELTA(cell_division_vector[0], 0.000, 1e-4);
        TS_ASSERT_DELTA(cell_division_vector[1], 0.000, 1e-4);
    }

    void TestCellsDividing() throw (Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(20, 20, true); // 21*21 nodes

        // Create a cell that keeps dividing
        MAKE_PTR(WildTypeCellMutationState, p_state);

        FixedDurationGenerationBasedCellCycleModel* p_model = new FixedDurationGenerationBasedCellCycleModel();
        p_model->SetCellProliferativeType(TRANSIT);
        p_model->SetMaxTransitGenerations(UINT_MAX);

        CellPtr p_cell(new Cell(p_state, p_model));
        p_cell->SetBirthTime(-13.5);

        std::vector<CellPtr> cells;
        cells.push_back(p_cell);

        std::vector<unsigned> real_node_indices;
        real_node_indices.push_back(220);

        // Create a cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetOutputDirectory("TestCellsDividing");
        simulator.SetDt(1);
        simulator.SetEndTime(50);

        // Run simulation
        simulator.Solve();

        TS_ASSERT_EQUALS(cell_population.GetNumRealCells(), 32u);
    }

    void TestDiffusionOfLargeNumberOfCells() throw (Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(49, 49, true); // 50*50 nodes

        // Create location indices
        std::vector<unsigned> real_node_indices;
        for (unsigned i=0; i<100; i++)
        {
            real_node_indices.push_back(i);
        }

        // Create cells
        std::vector<CellPtr> cells;
        CellsGenerator<FixedDurationGenerationBasedCellCycleModel, 2> cells_generator;
        cells_generator.GenerateBasic(cells, real_node_indices.size(), real_node_indices, DIFFERENTIATED);

        // Create a cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetOutputDirectory("TestDiffusionOfLargeNumberOfCells");
        simulator.SetDt(0.1);
        simulator.SetEndTime(5.0);

        // Pass an update rule to the simulation
        MAKE_PTR(DiffusionCaUpdateRule<2>, p_update_rule);
        simulator.AddUpdateRule(p_update_rule);

        // Run simulation
        simulator.Solve();

        TS_ASSERT_EQUALS(cell_population.GetNumRealCells(), 100u);
    }

    void TestDiffusionAndDeathOfLargeNumberOfCells() throw (Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(19, 19, true); // 50*50 nodes

        // Create location indices
        std::vector<unsigned> real_node_indices;
        for (unsigned i=0; i<100; i++)
        {
            real_node_indices.push_back(i);
        }

        // Create cells
        std::vector<CellPtr> cells;
        CellsGenerator<FixedDurationGenerationBasedCellCycleModel, 2> cells_generator;
        cells_generator.GenerateBasic(cells, real_node_indices.size(), real_node_indices, DIFFERENTIATED);

        // Create a cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetOutputDirectory("TestDiffusionAndDeathOfLargeNumberOfCells");
        simulator.SetDt(0.1);
        simulator.SetEndTime(10);

        // Pass an update rule to the simulation
        MAKE_PTR(DiffusionCaUpdateRule<2>, p_update_rule);
        simulator.AddUpdateRule(p_update_rule);

        // Create cell killer and pass in to simulation
        MAKE_PTR_ARGS(RandomCellKiller<2>, p_killer, (&cell_population, 0.005));
        simulator.AddCellKiller(p_killer);

        // Run simulation
        simulator.Solve();

        TS_ASSERT_EQUALS(cell_population.GetNumRealCells(), 95u);
    }

    void TestDiffusionAndDivisionOfLargeNumberOfCells() throw (Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(19, 19, true); // 50*50 nodes

        // Create cells
        std::vector<CellPtr> cells;
        CellsGenerator<FixedDurationGenerationBasedCellCycleModel, 2> cells_generator;
        cells_generator.GenerateBasic(cells, 50);

        std::vector<unsigned> real_node_indices;
        for (unsigned i=0; i<cells.size(); i++)
        {
            cells[i]->SetBirthTime(-RandomNumberGenerator::Instance()->ranf());
            real_node_indices.push_back(i);
        }

        // Create a cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetOutputDirectory("TestDiffusionAndDivisionOfLargeNumberOfCells");
        simulator.SetDt(0.1);
        simulator.SetEndTime(10);

        // Pass an update rule to the simulation
        MAKE_PTR(DiffusionCaUpdateRule<2>, p_update_rule);
        simulator.AddUpdateRule(p_update_rule);

        // Run simulation
        simulator.Solve();

        TS_ASSERT_EQUALS(cell_population.GetNumRealCells(), 50u);
    }

    void TestDiffusionAndAdvectionAndDivisionOfLargeNumberOfCells() throw (Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(49, 49, true); // 50*50 nodes

        // Create cells
        unsigned num_cells = 6;
        std::vector<CellPtr> cells;
        CellsGenerator<FixedDurationGenerationBasedCellCycleModel, 2> cells_generator;
        cells_generator.GenerateBasic(cells, num_cells);

        for (unsigned i=0; i<cells.size(); i++)
        {
            cells[i]->SetBirthTime(-RandomNumberGenerator::Instance()->ranf());
        }

        std::vector<unsigned> real_node_indices;
        real_node_indices.push_back(0);
        real_node_indices.push_back(1);
        real_node_indices.push_back(2);
        real_node_indices.push_back(50);
        real_node_indices.push_back(51);
        real_node_indices.push_back(100);

        // Create a cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetOutputDirectory("TestDiffusionAndAdvectionAndDivision");
        simulator.SetDt(0.1);
        simulator.SetEndTime(10);

        // Pass an update rule to the simulation
        MAKE_PTR(DiffusionCaUpdateRule<2>, p_update_rule);
        simulator.AddUpdateRule(p_update_rule);
        
        // Pass another update rule to the simulation
        MAKE_PTR_ARGS(AdvectionCaUpdateRule<2>, p_update_rule2, (7, 2.0));
        simulator.AddUpdateRule(p_update_rule2);

        // Run simulation
        TS_ASSERT_THROWS_NOTHING(simulator.Solve());
    }

    void TestMultipleAdvectionCaUpdateRules() throw(Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(6, 6, true); // 7*7 nodes

        // Create a single cell in the centre of the mesh
        MAKE_PTR(WildTypeCellMutationState, p_state);
        FixedDurationGenerationBasedCellCycleModel* p_model = new FixedDurationGenerationBasedCellCycleModel();
        p_model->SetCellProliferativeType(DIFFERENTIATED);
        CellPtr p_cell(new Cell(p_state, p_model));

        std::vector<CellPtr> cells;
        cells.push_back(p_cell);

        std::vector<unsigned> real_node_indices;
        real_node_indices.push_back(24);

        // Create cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetOutputDirectory("TestMultipleAdvectionCaUpdateRules");

        // Pass multiple advection update rules to the simulation, one for each direction
        MAKE_PTR_ARGS(AdvectionCaUpdateRule<2>, p_update_rule0, (0, 1.0));
        MAKE_PTR_ARGS(AdvectionCaUpdateRule<2>, p_update_rule1, (1, 1.0));
        MAKE_PTR_ARGS(AdvectionCaUpdateRule<2>, p_update_rule2, (2, 1.0));
        MAKE_PTR_ARGS(AdvectionCaUpdateRule<2>, p_update_rule3, (3, 1.0));
        MAKE_PTR_ARGS(AdvectionCaUpdateRule<2>, p_update_rule4, (4, 1.0));
        MAKE_PTR_ARGS(AdvectionCaUpdateRule<2>, p_update_rule5, (5, 1.0));
        MAKE_PTR_ARGS(AdvectionCaUpdateRule<2>, p_update_rule6, (6, 1.0));
        MAKE_PTR_ARGS(AdvectionCaUpdateRule<2>, p_update_rule7, (7, 1.0));
        simulator.AddUpdateRule(p_update_rule0);
        simulator.AddUpdateRule(p_update_rule1);
        simulator.AddUpdateRule(p_update_rule2);
        simulator.AddUpdateRule(p_update_rule3);
        simulator.AddUpdateRule(p_update_rule4);
        simulator.AddUpdateRule(p_update_rule5);
        simulator.AddUpdateRule(p_update_rule6);
        simulator.AddUpdateRule(p_update_rule7);

        /*
         * Set the time step to be large enough to guarantee that the cell moves
         * according to each of the update rules at each time step (and hence
         * remains at its original location).
         */
        simulator.SetDt(2.0);

        simulator.SetEndTime(10.0);

        // Run simulation
        simulator.Solve();

        // Test cell remains at centre of mesh
        AbstractCellPopulation<2>::Iterator cell_iter = simulator.rGetCellPopulation().Begin();
        c_vector<double, 2> cell_location = simulator.rGetCellPopulation().GetLocationOfCellCentre(*cell_iter);
        TS_ASSERT_DELTA(cell_location[0], 3.000, 1e-4);
        TS_ASSERT_DELTA(cell_location[1], 3.000, 1e-4);
    }

    void TestRandomIterationOverUpdateRules() throw (Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(10, 10, true); // 50*50 nodes

        // Create a single cell
        MAKE_PTR(WildTypeCellMutationState, p_state);
        FixedDurationGenerationBasedCellCycleModel* p_model = new FixedDurationGenerationBasedCellCycleModel();
        p_model->SetCellProliferativeType(STEM);
        CellPtr p_cell(new Cell(p_state, p_model));

        std::vector<CellPtr> cells;
        cells.push_back(p_cell);

        std::vector<unsigned> real_node_indices;
        real_node_indices.push_back(0);

        // Create a cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetOutputDirectory("TestRandomIterationOverUpdateRules");
        simulator.SetDt(0.1);
        simulator.SetEndTime(5.0);
        simulator.SetIterateRandomlyOverUpdateRuleCollection(true);
        simulator.SetIterateRandomlyOverCells(true);

        // Pass an update rule to the simulation
        MAKE_PTR_ARGS(DiffusionCaUpdateRule<2>, p_update_rule, (1.0)); // unit diffusion coefficient
        simulator.AddUpdateRule(p_update_rule);
        
        // Pass another update rule to the simulation
        MAKE_PTR_ARGS(AdvectionCaUpdateRule<2>, p_update_rule2, (0, 1.0)); // flow upward with unit mean speed
        simulator.AddUpdateRule(p_update_rule2);

        // Run simulation
        simulator.Solve();

        // Test final position of cell
        AbstractCellPopulation<2>::Iterator cell_iter = simulator.rGetCellPopulation().Begin();
        c_vector<double, 2> cell_location = simulator.rGetCellPopulation().GetLocationOfCellCentre(*cell_iter);
        TS_ASSERT_DELTA(cell_location[0], 0.000, 1e-4);
        TS_ASSERT_DELTA(cell_location[1], 3.000, 1e-4);
    }

    void TestStandardResultForArchivingTestsBelow() throw (Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(49, 49, true); // 50*50 nodes

        // Create cells
        std::vector<CellPtr> cells;
        std::vector<unsigned> real_node_indices;
        MAKE_PTR(WildTypeCellMutationState, p_state);
        unsigned num_cells = 100;
        for (unsigned i=0; i<num_cells; i++)
        {
            FixedDurationGenerationBasedCellCycleModel* p_model = new FixedDurationGenerationBasedCellCycleModel();
            p_model->SetCellProliferativeType(STEM);
            CellPtr p_cell(new Cell(p_state, p_model));
            cells.push_back(p_cell);

            real_node_indices.push_back(i);
        }

        // Create a cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);
        cell_population.SetOutputCellCyclePhases(true);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetOutputDirectory("CaBasedStandardResult");
        simulator.SetDt(0.1);
        simulator.SetEndTime(8.0);

        // Pass an update rule to the simulation
        MAKE_PTR(DiffusionCaUpdateRule<2>, p_update_rule);
        simulator.AddUpdateRule(p_update_rule);

        // Create cell killer and pass in to simulation
        MAKE_PTR_ARGS(RandomCellKiller<2>, p_killer, (&cell_population, 0.005));
        simulator.AddCellKiller(p_killer);

        // Run simulation
        simulator.Solve();

        TS_ASSERT_EQUALS(simulator.rGetCellPopulation().GetNumRealCells(), 96u);

        AbstractCellPopulation<2>::Iterator cell_iter = simulator.rGetCellPopulation().Begin();
        for (unsigned i=0; i<28; i++)
        {
            ++cell_iter;
        }
        c_vector<double, 2> cell_28_location = simulator.rGetCellPopulation().GetLocationOfCellCentre(*cell_iter);
        TS_ASSERT_DELTA(cell_28_location[0], 31.000, 1e-4);
        TS_ASSERT_DELTA(cell_28_location[1], 3.000, 1e-4);

        for (unsigned i=29; i<61; i++)
        {
            ++cell_iter;
        }
        c_vector<double, 2> cell_61_location = simulator.rGetCellPopulation().GetLocationOfCellCentre(*cell_iter);
        TS_ASSERT_DELTA(cell_61_location[0], 11.000, 1e-4);
        TS_ASSERT_DELTA(cell_61_location[1], 5.000, 1e-4);

        // Check writing of Voronoi data
        OutputFileHandler handler("CaBasedStandardResult", false);
        std::string results_file = handler.GetOutputDirectoryFullPath() + "results_from_time_0/cellcyclephases.dat";

        NumericFileComparison comp(results_file, "notforrelease_cell_based/test/data/CaBasedCellCyclePhaseOutput/cellcyclephases.dat");
        TS_ASSERT(comp.CompareFiles());
        TS_ASSERT_EQUALS(system(("diff " + results_file + " notforrelease_cell_based/test/data/CaBasedCellCyclePhaseOutput/cellcyclephases.dat").c_str()), 0);
    }

    // Testing Save
    void TestSave() throw (Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(49, 49, true); // 50*50 nodes

        // Create cells
        std::vector<CellPtr> cells;
        std::vector<unsigned> real_node_indices;
        MAKE_PTR(WildTypeCellMutationState, p_state);
        unsigned num_cells = 100;
        for (unsigned i=0; i<num_cells; i++)
        {
            FixedDurationGenerationBasedCellCycleModel* p_model = new FixedDurationGenerationBasedCellCycleModel();
            p_model->SetCellProliferativeType(STEM);
            CellPtr p_cell(new Cell(p_state, p_model));
            cells.push_back(p_cell);

            real_node_indices.push_back(i);
        }

        // Create a cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);
        cell_population.SetOutputCellCyclePhases(true);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetOutputDirectory("CaBasedSaveAndLoad");
        simulator.SetDt(0.1);

        // Pass an update rule to the simulation
        MAKE_PTR(DiffusionCaUpdateRule<2>, p_update_rule);
        simulator.AddUpdateRule(p_update_rule);

        // Our full end time is 8.0, here we run until 3.0 then load and run more below
        simulator.SetEndTime(3.0);

        // Create cell killer and pass in to simulation
        MAKE_PTR_ARGS(RandomCellKiller<2>,  p_killer, (&cell_population, 0.005));
        simulator.AddCellKiller(p_killer);

        // Run simulation
        simulator.Solve();

        TS_ASSERT_EQUALS(simulator.rGetCellPopulation().GetNumRealCells(), 98u);

        // Save the results
        CellBasedSimulationArchiver<2, CaBasedSimulation<2> >::Save(&simulator);
    }

    // Testing Load (based on previous two tests)
    void TestLoad() throw (Exception)
    {
        // Load the simulation from the TestSave method above and run it from 3.0 to 6.0
        CaBasedSimulation<2>* p_simulator1;

        p_simulator1 = CellBasedSimulationArchiver<2, CaBasedSimulation<2> >::Load("CaBasedSaveAndLoad", 3.0);

        TS_ASSERT_EQUALS(p_simulator1->rGetCellPopulation().GetNumRealCells(), 98u);
        TS_ASSERT_DELTA(p_simulator1->GetDt(), 0.1, 1e-6);

        p_simulator1->SetEndTime(6.0);
        p_simulator1->Solve();

        // Get mesh
        TetrahedralMesh<2,2>& r_mesh1 = (static_cast<CaBasedCellPopulation<2>*>(&(p_simulator1->rGetCellPopulation())))->rGetMesh();

        // Save then reload, compare meshes either side
        CellBasedSimulationArchiver<2, CaBasedSimulation<2> >::Save(p_simulator1);

        CaBasedSimulation<2>* p_simulator2 = CellBasedSimulationArchiver<2, CaBasedSimulation<2> >::Load("CaBasedSaveAndLoad", 6.0);
        TetrahedralMesh<2,2>& r_mesh2 = (static_cast<CaBasedCellPopulation<2>*>(&(p_simulator2->rGetCellPopulation())))->rGetMesh();

        CompareMeshes(&r_mesh1, &r_mesh2);

        // Run a bit further...
        p_simulator2->SetEndTime(8.0);

        // Run simulation
        p_simulator2->Solve();

        TS_ASSERT_EQUALS(p_simulator2->rGetCellPopulation().GetNumRealCells(), 96u);

        AbstractCellPopulation<2>::Iterator cell_iter = p_simulator2->rGetCellPopulation().Begin();
        for (unsigned i=0; i<28; i++)
        {
            ++cell_iter;
        }
        c_vector<double, 2> cell_28_location = p_simulator2->rGetCellPopulation().GetLocationOfCellCentre(*cell_iter);
        TS_ASSERT_DELTA(cell_28_location[0], 31.000, 1e-4);
        TS_ASSERT_DELTA(cell_28_location[1], 3.000, 1e-4);

        for (unsigned i=29; i<61; i++)
        {
            ++cell_iter;
        }
        c_vector<double, 2> cell_61_location = p_simulator2->rGetCellPopulation().GetLocationOfCellCentre(*cell_iter);
        TS_ASSERT_DELTA(cell_61_location[0], 11.000, 1e-4);
        TS_ASSERT_DELTA(cell_61_location[1], 5.000, 1e-4);

        // Tidy up
        delete p_simulator1;
        delete p_simulator2;
    }

    void TestExceptions() throw (Exception)
    {
        // Create mesh
        TetrahedralMesh<2,2> mesh;
        mesh.ConstructRectangularMesh(49, 49, true); // 50*50 nodes

        // Create location indices
        std::vector<unsigned> real_node_indices;
        for (unsigned i=0; i<100; i++)
        {
            real_node_indices.push_back(i);
        }

        // Create cells
        std::vector<CellPtr> cells;
        CellsGenerator<FixedDurationGenerationBasedCellCycleModel, 2> cells_generator;
        cells_generator.GenerateBasic(cells, real_node_indices.size(), real_node_indices, DIFFERENTIATED);

        // Create a cell population
        CaBasedCellPopulation<2> cell_population(mesh, cells, real_node_indices);

        // Set up cell-based simulation
        CaBasedSimulation<2> simulator(cell_population);
        simulator.SetOutputDirectory("TestDiffusionOfLargeNumberOfCells");
        simulator.SetDt(0.1);
        simulator.SetEndTime(5.0);

        // Pass an update rule to the simulation
        MAKE_PTR(DiffusionCaUpdateRule<2>, p_update_rule);
        simulator.AddUpdateRule(p_update_rule);

        // Test that the simulation parameters are output correctly
        std::string output_directory = "TestCaBasedSimulationOutputParameters";
        OutputFileHandler output_file_handler(output_directory, false);
        out_stream parameter_file = output_file_handler.OpenOutputFile("ca_simulation_results.parameters");

        simulator.OutputSimulationParameters(parameter_file);
        parameter_file->close();

        std::string results_dir = output_file_handler.GetOutputDirectoryFullPath();
        TS_ASSERT_EQUALS(system(("diff " + results_dir + "ca_simulation_results.parameters  notforrelease_cell_based/test/data/TestCaSimulationOutputParameters/ca_simulation_results.parameters").c_str()), 0);
    }
};

#endif /*TESTONLATTICESIMULATIONWITHCABASEDCELLPOPULATION_HPP_*/