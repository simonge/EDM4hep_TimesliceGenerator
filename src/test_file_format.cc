#include <iostream>
#include <memory>
#include <TFile.h>
#include <TTree.h>

// Simple test to check if we can load a generated file with ROOT/TTree
// This is a basic format verification without podio dependencies
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <root_file>" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    std::cout << "=== Basic ROOT File Format Test ===" << std::endl;
    std::cout << "Testing file: " << filename << std::endl;
    
    // Try to open the file
    auto file = std::make_unique<TFile>(filename.c_str(), "READ");
    if (!file || file->IsZombie()) {
        std::cout << "❌ ERROR: Could not open file: " << filename << std::endl;
        return 1;
    }
    
    std::cout << "✓ File opened successfully" << std::endl;
    
    // List all objects in the file
    std::cout << "\n--- File Contents ---" << std::endl;
    file->ls();
    
    // Check for expected trees
    TTree* events_tree = dynamic_cast<TTree*>(file->Get("events"));
    if (!events_tree) {
        std::cout << "❌ ERROR: No 'events' tree found" << std::endl;
        return 1;
    }
    
    std::cout << "\n✓ Found 'events' tree with " << events_tree->GetEntries() << " entries" << std::endl;
    
    // Check for podio_metadata tree
    TTree* metadata_tree = dynamic_cast<TTree*>(file->Get("podio_metadata"));
    if (metadata_tree) {
        std::cout << "✓ Found 'podio_metadata' tree with " << metadata_tree->GetEntries() << " entries" << std::endl;
    } else {
        std::cout << "⚠️  No 'podio_metadata' tree found" << std::endl;
    }
    
    // List branches in events tree
    std::cout << "\n--- Events Tree Branches ---" << std::endl;
    TObjArray* branches = events_tree->GetListOfBranches();
    if (branches) {
        std::cout << "Total branches: " << branches->GetEntries() << std::endl;
        
        int contrib_branches = 0;
        int collections = 0;
        
        for (int i = 0; i < branches->GetEntries(); ++i) {
            TBranch* branch = (TBranch*)branches->At(i);
            if (branch) {
                std::string branch_name = branch->GetName();
                std::cout << "  " << branch_name;
                
                if (branch_name.find("_contributions") != std::string::npos) {
                    contrib_branches++;
                    std::cout << " [CONTRIBUTION BRANCH]";
                } else if (branch_name.find("_") != 0) {  // Not a reference branch
                    collections++;
                    std::cout << " [COLLECTION]";
                }
                
                std::cout << std::endl;
            }
        }
        
        std::cout << "\nSummary:" << std::endl;
        std::cout << "  Collections: " << collections << std::endl;
        std::cout << "  Contribution branches: " << contrib_branches << std::endl;
        
        if (contrib_branches > 0) {
            std::cout << "✓ Found contribution relationship branches" << std::endl;
        } else {
            std::cout << "⚠️  No contribution relationship branches found" << std::endl;
        }
    }
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    return 0;
}