#ifndef __CLUSTERING_H
#define __CLUSTERING_H

#include <aol.h>
#include <vec.h>
#include <vectorExtensions.h>
#include <randomGenerator.h>


class KMEANS_INIT_METHOD {
public:
  static const int RNG  = 0;    // Initialize clusters with random pairwise different points from the given data
  static const int PCA  = 1;    // Initialize clusters with first principal components
  static const int MEC  = 2;    // Initialize clusters with maximum edge-weighted clique within the complete graph spanned by the data with Euclidean distances as edge weights
  
  static const int NUM  = 3;
  
  static std::string getIdentifier ( const int InitMethod ) {
    if ( InitMethod == RNG ) return         "rng";
    else if ( InitMethod == PCA ) return    "pca";
    else if ( InitMethod == MEC ) return    "mec";
    else throw aol::Exception ( "Did not recognize initialization method!", __FILE__, __LINE__ );
  }
  
  static std::string toString ( const int InitMethod ) {
    if ( InitMethod == RNG ) return         "Random";
    else if ( InitMethod == PCA ) return    "Principal Component Analysis";
    else if ( InitMethod == MEC ) return    "Maximum edge-weighted clique";
    else throw aol::Exception ( "Did not recognize initialization method!", __FILE__, __LINE__ );
  }
};


/**
 * \brief Provides a class for k-means clustering
 * 
 * The class can be applied to a vector of scalars and find k scalar clusters
 * It can also be applied to a multivector and find k vector-valued clusters
 *
 * \author Mevenkamp
 */
template <typename _RealType>
class KMeansClusterer {
  typedef _RealType RealType;
protected:
  bool _quietMode;
public:
  KMeansClusterer ( ) : _quietMode ( true ) { }
  
  // Cluster multiple times each time initializing with RNG and return best result (without cluster labels)
  RealType applyMultipleRNG ( const aol::MultiVector<RealType> &Input, aol::MultiVector<RealType> &Clusters, const short NumClusters,
                              const int NumPasses = 100, const int MaxIt = 100, const RealType Epsilon = 0.001 ) {
    aol::Vector<int> clusterLabels;
    return applyMultipleRNG ( Input, Clusters, NumClusters, clusterLabels, NumPasses, MaxIt, Epsilon );
  }
  
  // Cluster multiple times each time initializing with RNG and return best result
  RealType applyMultipleRNG ( const aol::MultiVector<RealType> &Input, aol::MultiVector<RealType> &Clusters, const short NumClusters, aol::Vector<int> &ClusterLabels,
                              const int NumPasses = 100, const int MaxIt = 100, const RealType Epsilon = 0.001 ) {
    RealType leastResidual = aol::NumberTrait<RealType>::Inf, residual;
    aol::MultiVector<RealType> bestClusters ( Input.numComponents ( ), NumClusters );
    aol::Vector<int> bestClusterLabels ( Input[0].size ( ) );
    for ( int i=0; i<NumPasses ; ++i ) {
      residual = apply ( Input, Clusters, NumClusters, ClusterLabels, KMEANS_INIT_METHOD::RNG, MaxIt, Epsilon );
      if ( residual < leastResidual ) {
        leastResidual = residual;
        bestClusters = Clusters;
        bestClusterLabels = ClusterLabels;
      }
    }
    
    return leastResidual;
  }

  // Clustering of vectors (without output of cluster labels)
  RealType apply ( const aol::MultiVector<RealType> &Input, aol::MultiVector<RealType> &Clusters, const short NumClusters,
                   const int InitMethod = KMEANS_INIT_METHOD::RNG, const int MaxIt = 100, const RealType Epsilon = 0.001 ) {
    aol::Vector<int> clusterLabels;
    return apply ( Input, Clusters, NumClusters, clusterLabels, InitMethod, MaxIt, Epsilon );
  }
  
  // Clustering of vectors (including output of cluster labels for each input vector)
  RealType apply ( const aol::MultiVector<RealType> &Input, aol::MultiVector<RealType> &Clusters, const short NumClusters, aol::Vector<int> &ClusterLabels,
                   const int InitMethod = KMEANS_INIT_METHOD::RNG, const int MaxIt = 100, const RealType Epsilon = 0.001 ) {
    const int dim = Input.numComponents ( );
    if ( dim <= 0 ) throw aol::Exception ( "Received empty MultiVector, but expected at least one component!", __FILE__, __LINE__ );
    
    ClusterLabels.resize ( Input[0].size ( ) );
    aol::MultiVector<RealType> clustersNew ( dim, NumClusters );
    initializeClusters ( Input, clustersNew, InitMethod );
    
    // Refine clusters until difference between old and new clusters is less than Epsilon
    aol::MultiVector<RealType> clustersOld ( dim, NumClusters ), clustersDiff ( dim, NumClusters );
    aol::Vector<int> clusterSizes ( NumClusters );
    int numIt = 0;
    RealType residual;
    aol::Vector<RealType> val ( dim );
    do {
      clustersOld = clustersNew;
      
      // Update clusters
      clusterSizes.setZero ( );
      clustersNew.setZero ( );
      for ( int j=0; j<Input[0].size ( ) ; ++j ) {
        Input.getTo ( j, val );
        const int nearestClusterIdx = getNearestClusterIndex ( val, clustersOld );
        ClusterLabels[j] = nearestClusterIdx;
        clusterSizes[nearestClusterIdx]++;
        for ( int c=0; c<dim ; ++c )
          clustersNew[c][nearestClusterIdx] += Input[c][j];
      }
      for ( int c=0; c<dim ; ++c )
        for ( int i=0; i<clustersNew[0].size ( ) ; ++i )
          if ( clusterSizes[i] > 0 ) clustersNew[c][i] /= clusterSizes[i];
      
      // Calculate difference between old and new clusters
      clustersDiff = clustersOld;
      clustersDiff -= clustersNew;
      numIt++;
      
      // Calculate residual
      residual = 0.0;
      for ( int j=0; j<Input[0].size ( ) ; ++j )
        for ( int c=0; c<dim ; ++c )
          residual += aol::Sqr<RealType> ( clustersNew[c][ClusterLabels[j]] - Input[c][j] );
      residual = sqrt ( residual );
      
      if ( !_quietMode ) cerr << "KMeans: Iteration " << numIt << ": " << clustersNew << endl;
    } while ( clustersDiff.norm ( ) > Epsilon && numIt < MaxIt );
    
    Clusters.resize ( Input.numComponents ( ), NumClusters );
    Clusters = clustersNew;
    
    return residual;
  }
  
  // Clustering of scalars (without output of cluster labels)
  RealType apply ( const aol::Vector<RealType> &Input, aol::Vector<RealType> &Clusters, const short NumClusters,
               const int InitMethod = KMEANS_INIT_METHOD::RNG, const int MaxIt = 100, const RealType Epsilon = 0.001 ) {
    aol::Vector<int> clusterLabels;
    return apply ( Input, Clusters, NumClusters, clusterLabels, InitMethod, MaxIt, Epsilon );
  }
  
  // Clustering of scalars (including output of cluster labels for each input element)
  RealType apply ( const aol::Vector<RealType> &Input, aol::Vector<RealType> &Clusters, const short NumClusters, aol::Vector<int> &ClusterLabels,
               const int InitMethod = KMEANS_INIT_METHOD::RNG, const int MaxIt = 100, const RealType Epsilon = 0.001 ) {
    aol::MultiVector<RealType> input ( 1, Input.size ( ) );
    input[0] = Input;
    aol::MultiVector<RealType> clusters;
    RealType residual = apply ( input, clusters, NumClusters, ClusterLabels, InitMethod, MaxIt, Epsilon );
    Clusters.resize ( NumClusters );
    Clusters = clusters[0];
    return residual;
  }
  
  void initializeClusters ( const aol::MultiVector<RealType> &Input, aol::MultiVector<RealType> &Clusters, const int InitMethod ) const {
    const int numClusters = Clusters[0].size ( );
    const int dim = Input.numComponents ( );
    
    if ( InitMethod == KMEANS_INIT_METHOD::RNG ) {
      // Initialize clusters randomly
      aol::Vector<int> randClusterIndices ( numClusters );
      aol::RandomGenerator randomGenerator;
//      randomGenerator.randomize ( );    // if "true" randomness is required, uncomment this line
      randomGenerator.rIntVecPairwiseDifferent ( randClusterIndices, 0, Input[0].size ( ) );
      for ( int c=0; c<dim ; ++c )
        for ( int i=0; i<numClusters; ++i )
          Clusters[c][i] = Input[c][randClusterIndices[i]];
    } else if ( InitMethod == KMEANS_INIT_METHOD::PCA ) {
      throw aol::UnimplementedCodeException ( "Not implemented", __FILE__, __LINE__ );
    } else if ( InitMethod == KMEANS_INIT_METHOD::MEC ) {
      // Initialize clusters with maximum edge-weighted clique within graph spanned by the data points with Euclidean distances as edge weights
      // Exact solution is NP hard to obtain, thus approximate with O(N) greedy algorithm
      // Step 1: Add point with largest norm
      RealType maxNorm = 0;
      int maxIdx = 0;
      aol::Vector<RealType> vec ( dim );
      for ( int i=0; i<Input[0].size ( ) ; ++i ) {
        Input.getTo ( i, vec );
        if ( vec.norm ( ) > maxNorm ) {
          maxIdx = i;
          maxNorm = vec.norm ( );
        }
      }
      for ( int c=0; c<dim ; ++c ) Clusters[c][0] = Input[c][maxIdx];
      // Step 2: Iteratively add points that result in the MEC for the current size, i.e. the points with largest summed distance to all points already in the clique
      for ( int k=1 ; k<numClusters ; ++k ) {
        RealType maxMinDist = 0;
        int maxIdx = 0;
        aol::Vector<RealType> cluster ( dim );
        for ( int i=0; i<Input[0].size ( ) ; ++i ) {
          aol::Vector<RealType> dists ( k );
          for ( int j=0; j<k ; ++j ) {
            Input.getTo ( i, vec );
            Clusters.getTo ( j, cluster );
            vec -= cluster;
            dists[j] = vec.norm ( );
          }
          if ( dists.getMinValue ( ) > maxMinDist ) {
            maxIdx = i;
            maxMinDist = dists.getMinValue ( );
          }
        }
        for ( int c=0; c<dim ; ++c ) Clusters[c][k] = Input[c][maxIdx];
      }
    } else throw aol::Exception ( "Did not recognize k-means initialization method!", __FILE__, __LINE__ );
  }
  
  int getNearestClusterIndex ( const aol::Vector<RealType> &Value, const aol::MultiVector<RealType> &Clusters ) {
    RealType minDistance = aol::NumberTrait<RealType>::Inf;
    int dim = Clusters.numComponents ( ), nearestClusterIdx = -1;
    for ( int i=0; i<Clusters[0].size ( ); ++i ) {
      RealType distance = 0;
      for ( int c=0; c<dim ; ++c ) distance += aol::Sqr<RealType> ( Clusters[c][i] - Value[c] );
      if ( distance < minDistance ) {
        minDistance = distance;
        nearestClusterIdx = i;
      }
    }
    
    if ( nearestClusterIdx < 0 )
      throw aol::Exception ( "Distance of specified Value to each Cluster is Infinity", __FILE__, __LINE__ );
    
    return nearestClusterIdx;
  }
  
  void setQuietMode ( bool Quiet ) {
    _quietMode = Quiet;
  }
};


/**
 * \author Mevenkamp
 */
template <typename _RealType, typename _PictureType>
class KMeans2DSpatialIntensityClusterer {
  typedef _RealType RealType;
  typedef _PictureType PictureType;
protected:
  bool _quietMode;
public:
  KMeans2DSpatialIntensityClusterer ( ) : _quietMode ( true ) { }

  void apply ( const PictureType &Arg, aol::RandomAccessContainer<aol::Vec2<RealType> > &Dest, const short NumClusters, const int MaxIt = 100, const RealType Epsilon = 0.001 ) {
    // Initialize class member variables
    aol::RandomAccessContainer<aol::Vec2<RealType> > clustersOld ( NumClusters ), clustersNew ( NumClusters );
    aol::Vec2<RealType> clustersPosDiff;
    RealType clustersDiff;
    aol::Vector<RealType> clusterSizes ( NumClusters );

    // Initialize clusters
    aol::MultiVector<int> randomPixels ( 2, NumClusters );
    aol::RandomGenerator randomGenerator;
//    randomGenerator.randomize ( );    // if "true" randomness is required, uncomment this line
    aol::Vector<int> min ( 2 ), max ( 2 );
    max[0] = Arg.getNumX ( ), max[1] = Arg.getNumY ( );
    randomGenerator.rIntMultiVecPairwiseDifferent ( randomPixels, min, max );
    for ( int i=0; i<NumClusters; ++i )
      clustersNew[i].set ( randomPixels[0][i], randomPixels[1][i] );

    int numIt = 0;
    aol::Vec2<short> pos;
    do {
      clustersOld = clustersNew;
      clusterSizes.setZero ( );
      for ( int i=0; i<clustersNew.size ( ) ; ++i ) clustersNew[i].setZero ( );
      for ( short i=0; i<Arg.getNumX ( ) ; ++i ) {
        for ( short j=0; j<Arg.getNumY ( ) ; ++j ) {
          pos.set ( i, j );
          const int nearestClusterIdx = getNearestClusterIndex ( pos, clustersOld, Arg );
          clusterSizes[nearestClusterIdx] += exp ( Arg.get ( pos ) );
          clustersNew[nearestClusterIdx][0] += pos[0] * exp ( Arg.get ( pos ) );
          clustersNew[nearestClusterIdx][1] += pos[1] * exp ( Arg.get ( pos ) );
        }
      }
      clustersDiff = 0;
      for ( int i=0; i<clustersNew.size ( ) ; ++i ) {
        clustersNew[i] /= clusterSizes[i];
        clustersPosDiff = clustersNew[i];
        clustersPosDiff -= clustersOld[i];
        clustersDiff += clustersPosDiff.normSqr ( );
      }
      clustersDiff = sqrt ( clustersDiff );

      numIt++;

      if ( !_quietMode ) std::cerr << "KMeans: Iteration " << numIt << ": " << std::endl;
      for ( int i=0; i<clustersNew.size ( ) ; ++i )
        cerr << clustersNew[i] << endl;
    } while ( clustersDiff > Epsilon && numIt < MaxIt );

    Dest.reallocate ( NumClusters );
    Dest = clustersNew;
  }

private:
  int getNearestClusterIndex ( const aol::Vec2<short> &Pos, const aol::RandomAccessContainer<aol::Vec2<RealType> > &Clusters, const PictureType &Data ) {
    RealType minDistance = aol::NumberTrait<RealType>::Inf;
    int nearestClusterIdx = -1;
    aol::Vec2<RealType> posDiff;
    for ( int i=0; i < Clusters.size ( ); ++i ) {
      posDiff.set ( Pos[0], Pos[1] );
      posDiff -= Clusters[i];
      RealType distance = aol::Abs ( exp ( Data.get ( round ( Clusters[i][0] ), round ( Clusters[i][0] ) ) ) - exp ( Data.get ( Pos ) ) ) * posDiff.norm ( );
      if ( distance < minDistance ) {
        minDistance = distance;
        nearestClusterIdx = i;
      }
    }
    return nearestClusterIdx;
  }
};

#endif /* CLUSTERING_H_ */
