/// \file
/// \ingroup tutorial_tmva
/// \notebook -nodraw
/// This macro provides an example of how to use TMVA for k-folds cross
/// evaluation.
///
/// As input data is used a toy-MC sample consisting of two guassian
/// distributions.
///
/// The output file "TMVA.root" can be analysed with the use of dedicated
/// macros (simply say: root -l <macro.C>), which can be conveniently
/// invoked through a GUI that will appear at the end of the run of this macro.
/// Launch the GUI via the command:
///
/// ```
/// root -l -e 'TMVA::TMVAGui("TMVA.root")'
/// ```
///
/// ## Cross Evaluation
/// Cross evaluation is a special case of k-folds cross validation where the
/// splitting into k folds is computed deterministically. This ensures that the
/// a given event will always end up in the same fold.
///
/// In addition all resulting classifiers are saved and can be applied to new
/// data using `MethodCrossValidation`. One requirement for this to work is a
/// splitting function that is evaluated for each event to determine into what
/// fold it goes (for training/evaluation) or to what classifier (for
/// application).
///
/// ## Split Expression
/// Cross evaluation uses a deterministic split to partition the data into
/// folds called the split expression. The expression can be any valid
/// `TFormula` as long as all parts used are defined.
///
/// For each event the split expression is evaluated to a number and the event
/// is put in the fold corresponding to that number.
///
/// It is recommended to always use `%int([NumFolds])` at the end of the
/// expression.
///
/// The split expression has access to all spectators and variables defined in
/// the dataloader. Additionally, the number of folds in the split can be
/// accessed with `NumFolds` (or `numFolds`).
///
/// ### Example
///  ```
///  "int(fabs([eventID]))%int([NumFolds])"
///  ```
///
/// - Project   : TMVA - a ROOT-integrated toolkit for multivariate data analysis
/// - Package   : TMVA
/// - Root Macro: TMVACrossValidation
///
/// \macro_output
/// \macro_code
/// \author Kim Albertsson (adapted from code originally by Andreas Hoecker)

#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

#include "TChain.h"
#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include "TObjString.h"
#include "TSystem.h"
#include "TROOT.h"

#include "TMVA/Factory.h"
#include "TMVA/DataLoader.h"
#include "TMVA/Tools.h"
#include "TMVA/TMVAGui.h"

// Helper function to load data into TTrees.
TTree *genTree(Int_t nPoints, Double_t offset, Double_t scale, UInt_t seed = 100)
{
   TRandom3 rng(seed);
   Float_t x = 0;
   Float_t y = 0;
   UInt_t eventID = 0;

   TTree *data = new TTree();
   data->Branch("x", &x, "x/F");
   data->Branch("y", &y, "y/F");
   data->Branch("eventID", &eventID, "eventID/I");

   for (Int_t n = 0; n < nPoints; ++n) {
      x = rng.Gaus(offset, scale);
      y = rng.Gaus(offset, scale);

      // For our simple example it is enough that the id's are uniformly
      // distributed and independent of the data.
      ++eventID;

      data->Fill();
   }

   // Important: Disconnects the tree from the memory locations of x and y.
   data->ResetBranchAddresses();
   return data;
}

int TMVACrossValidation()
{
   // This loads the library
   TMVA::Tools::Instance();

   // --------------------------------------------------------------------------

   // Load the data into TTrees. If you load data from file you can use a
   // variant of
   // ```
   // TString filename = "/path/to/file";
   // TFile * input = TFile::Open( filename );
   // TTree * signalTree = (TTree*)input->Get("TreeName");
   // ```
   TTree *sigTree = genTree(1000, 1.0, 1.0, 100);
   TTree *bkgTree = genTree(1000, -1.0, 1.0, 101);

   // Create a ROOT output file where TMVA will store ntuples, histograms, etc.
   TString outfileName("TMVA.root");
   TFile *outputFile = TFile::Open(outfileName, "RECREATE");

   // DataLoader definitions; We declare variables in the tree so that TMVA can
   // find them. For more information see TMVAClassification tutorial.
   TMVA::DataLoader *dataloader = new TMVA::DataLoader("dataset");

   // Data variables
   dataloader->AddVariable("x", 'F');
   dataloader->AddVariable("y", 'F');

   // Spectator used for split
   dataloader->AddSpectator("eventID", 'I');

   // Attaches the trees so they can be read from
   dataloader->AddSignalTree(sigTree, 1.0);
   dataloader->AddBackgroundTree(bkgTree, 1.0);

   // The CV mechanism of TMVA splits up the training set into several folds.
   // The test set is currently left unused. The `nTest_ClassName=1` assigns
   // one event to the the test set for each class and puts the rest in the
   // training set. A value of 0 is a special value and would split the
   // datasets 50 / 50.
   dataloader->PrepareTrainingAndTestTree("", "",
                                          "nTest_Signal=1"
                                          ":nTest_Background=1"
                                          ":SplitMode=Random"
                                          ":NormMode=NumEvents"
                                          ":!V");

   // --------------------------------------------------------------------------

   //
   // This sets up a CrossValidation class (which wraps a TMVA::Factory
   // internally) for 2-fold cross validation.
   //
   UInt_t numFolds = 2;
   TString analysisType = "Classification";
   TString splitExpr = "";

   //
   // One can also use a custom splitting function for producing the folds.
   // The example uses a dataset spectator `eventID`.
   //
   // The idea here is that eventID should be an event number that is integral,
   // random and independent of the data, generated only once. This last
   // property ensures that if a calibration is changed the same event will
   // still be assigned the same fold.
   // 
   // This can be used to use the cross validated classifiers in application,
   // a technique that can simplify statistical analysis.
   // 
   // If you want to run TMVACrossValidationApplication, make sure you have 
   // run this tutorial with the below line uncommented first.
   // 

   // TString splitExpr = "int(fabs([eventID]))%int([NumFolds])";

   TString cvOptions = Form("!V"
                            ":!Silent"
                            ":ModelPersistence"
                            ":AnalysisType=%s"
                            ":NumFolds=%i"
                            ":SplitExpr=%s",
                            analysisType.Data(), numFolds, splitExpr.Data());

   TMVA::CrossValidation ce{"TMVACrossValidation", dataloader, outputFile, cvOptions};

   // --------------------------------------------------------------------------

   //
   // Books a method to use for evaluation
   //
   ce.BookMethod(TMVA::Types::kBDT, "BDTG",
                 "!H:!V:NTrees=100:MinNodeSize=2.5%:BoostType=Grad:"
                 "Shrinkage=0.10:nCuts=20:MaxDepth=2");

   ce.BookMethod(TMVA::Types::kFisher, "Fisher",
                 "H:!V:Fisher:VarTransform=None:CreateMVAPdfs:"
                 "PDFInterpolMVAPdf=Spline2:NbinsMVAPdf=50:NsmoothMVAPdf=10");

   // --------------------------------------------------------------------------

   //
   // Train, test and evaluate the booked methods.
   // Evaluates the booked methods once for each fold and aggregates the result
   // in the specified output file.
   //
   ce.Evaluate();

   // --------------------------------------------------------------------------

   //
   // Process some output programatically, printing the ROC score for each
   // booked method.
   //
   auto bdtg_result = ce.GetResults()[0];
   std::cout << "==> BDTG ROC: avg (std): " << bdtg_result.GetROCAverage() << " ("
             << bdtg_result.GetROCStandardDeviation() << ")"
             << "" << std::endl;

   auto fisher_result = ce.GetResults()[1];
   std::cout << "==> Fisher ROC: avg (std): " << fisher_result.GetROCAverage() << " ("
             << fisher_result.GetROCStandardDeviation() << ")"
             << "" << std::endl;

   // --------------------------------------------------------------------------

   //
   // Save the output
   //
   outputFile->Close();

   std::cout << "==> Wrote root file: " << outputFile->GetName() << std::endl;
   std::cout << "==> TMVACrossValidation is done!" << std::endl;

   // --------------------------------------------------------------------------

   //
   // Launch the GUI for the root macros
   //
   if (!gROOT->IsBatch()) {
      TMVA::TMVAGui(outfileName);
   }

   return 0;
}

//
// This is used if the macro is compiled. If run through ROOT with
// `root -l -b -q MACRO.C` or similar it is unused.
//
int main(int argc, char **argv)
{
   TMVACrossValidation();
}
