#include "blockOperators1D.h"

const char* const DWT::dwts[] = {
  "haar", "db1", "db2", "db3", "db4", "db5", "db6", "db7", "db8", "db9", "db10",
          "db11", "db12", "db13", "db14", "db15",                                         // Daubechies
  
  "bior1.1", "bior1.3", "bior1.5", "bior2.2", "bior2.4", "bior2.6", "bior2.8",
  "bior3.1", "bior3.3", "bior3.5", "bior3.7", "bior3.9", "bior4.4", "bior5.5", "bior6.8", // Bi-orthogonal
  
  "coif1", "coif2", "coif3", "coif4", "coif5",                                            // Coiflets
  
  "sym2", "sym3", "sym4", "sym5", "sym6", "sym7", "sym8", "sym9", "sym10"                 // Symmlets
};

template class LinearUnitary1DTransformOp<double>;