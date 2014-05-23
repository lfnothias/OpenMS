// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2014.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Stephan Aiche $
// $Authors: Stephan Aiche, Chris Bielow $
// --------------------------------------------------------------------------

#ifndef OPENMS_ANALYSIS_QUANTITATION_ISOBARICCHANNELEXTRACTOR_H
#define OPENMS_ANALYSIS_QUANTITATION_ISOBARICCHANNELEXTRACTOR_H

#include <OpenMS/ANALYSIS/QUANTITATION/IsobaricQuantitationMethod.h>

#include <OpenMS/DATASTRUCTURES/DefaultParamHandler.h>

#include <OpenMS/KERNEL/MSExperiment.h>
#include <OpenMS/KERNEL/ConsensusMap.h>

namespace OpenMS
{
  /**
    @brief Extracts individual channels from MS/MS spectra for isobaric labeling experiments.

    @htmlinclude OpenMS_IsobaricChannelExtractor.parameters
  */
  class OPENMS_DLLAPI IsobaricChannelExtractor :
    public DefaultParamHandler
  {
public:
    /**
      @brief C'tor to create a new channel extractor for the given quantitation method.

      @param quant_method IsobaricQuantitationMethod providing the necessary information which channels should be extracted.
    */
    explicit IsobaricChannelExtractor(const IsobaricQuantitationMethod* const quant_method);

    /// Copy c'tor
    IsobaricChannelExtractor(const IsobaricChannelExtractor& other);

    /// Assignment operator
    IsobaricChannelExtractor& operator=(const IsobaricChannelExtractor& rhs);

    /**
      @brief Extracts the isobaric channels from the tandem MS data and stores intensity values in a consensus map.

      @param ms_exp_data Raw data to search for isobaric quantitation channels.
      @param consensus_map Output map containing the identified channels and the corresponding intensities.
    */
    void extractChannels(const MSExperiment<Peak1D>& ms_exp_data, ConsensusMap& consensus_map);

private:
    /**
      @brief Small struct to capture the current state of the purity computation.
    */
    struct PuritySate
    {
      /// Iterator pointing to the potential ms1 precursor scan
      MSExperiment<Peak1D>::ConstIterator precursorScan;
      /// Iterator pointing to the potential follow up ms1 scan
      MSExperiment<Peak1D>::ConstIterator followUpScan;
      /// Iterator pointing to the potential follow up ms1 scan
      MSExperiment<Peak1D>::ConstIterator activeScan;

      /// Indicates if a precursor was found
      bool hasPrecursorScan;
      /// Indicates if a follow up scan was found
      bool hasFollowUpScan;
      /// reference to the experiment to analyze
      const MSExperiment<Peak1D>& baseExperiment;

      PuritySate(const MSExperiment<>& targetExp) :
        baseExperiment(targetExp)
      {
        precursorScan = baseExperiment.end();
        hasPrecursorScan = false;

        // find the first ms1 scan in the experiment
        followUpScan = baseExperiment.begin();
        while (followUpScan->getMSLevel() != 1 && followUpScan != baseExperiment.end())
        {
          ++followUpScan;
        }

        // check if we found one
        hasFollowUpScan = followUpScan != baseExperiment.end();
      }

      void advanceFollowUp(const double rt)
      {
        // advance follow up scan until we found a ms1 scan with a bigger RT
        while (followUpScan->getMSLevel() != 1 &&
               followUpScan != baseExperiment.end() &&
               followUpScan->getRT() < rt)
        {
          ++followUpScan;
        }

        // check if we found one
        hasFollowUpScan = followUpScan != baseExperiment.end();
      }

      bool followUpValid(const double rt)
      {
        return hasFollowUpScan ? rt < followUpScan->getRT() : true;
      }

    };

    /// The used quantitation method (itraq4plex, tmt6plex,..).
    const IsobaricQuantitationMethod* quant_method_;

    /// Used to select only specific types of spectra for the channel extraction.
    String selected_activation_;

    /// Allowed deviation between the expected and observed reporter ion m/z.
    Peak2D::CoordinateType reporter_mass_shift_;

    /// Minimum intensity of the precursor to be considered for quantitation.
    Peak2D::IntensityType min_precursor_intensity_;

    /// Flag if precursor with missing intensity value or missing precursor spectrum should be included or not.
    bool keep_unannotated_precursor_;

    /// Minimum reporter ion intensity to be considered for quantitation.
    Peak2D::IntensityType min_reporter_intensity_;

    /// Flag if complete quantification should be discarded if a single reporter ion has an intensity below the threshold given in IsobaricChannelExtractor::min_reporter_intensity_ .
    bool remove_low_intensity_quantifications_;

    /// Minimum precursor purity to accept the spectrum for quantitation.
    double min_precursor_purity_;

    /// Max. allowed deviation between theoretical and observed isotopic peaks of the precursor peak in the isolation window to be counted as part of the precursor.
    double max_precursor_isotope_deviation_;

    /// add channel information to the map after it has been filled
    void registerChannelsInOutputMap_(ConsensusMap& consensus_map);

    /**
      @brief Checks if the given precursor fulfills all constraints for extractions.

      @param precursor The precursor to test.
      @return $true$ if the precursor can be used for extraction, $false$ otherwise.
    */
    bool isValidPrecursor_(const Precursor& precursor) const;

    /**
      @brief Checks whether the given ConsensusFeature contains a channel that is below the given intensity threshold.

      @param cf The ConsensusFeature to check.
      @return $true$ if a low intensity reporter is contained, $false$ otherwise.
    */
    bool hasLowIntensityReporter_(const ConsensusFeature& cf) const;

    /**
      @brief Computes the purity of the precursor given an iterator pointing to the MS/MS spectrum and one to the precursor spectrum.

      @param ms2_spec Iterator pointing to the ms2 spectrum.
      @param precursor Iterator pointing to the precursor spectrum of ms2_spec.
      @return Fraction of the total intensity in the isolation window of the precursor spectrum that was assigned to the precursor.
    */
    double computePrecursorPurity_(const MSExperiment<Peak1D>::ConstIterator& ms2_spec, const PuritySate& pState) const;

    /**
      @brief Computes the purity of the precursor given an iterator pointing to the MS/MS spectrum and a reference to the potential precursor spectrum.

      @param ms2_spec Iterator pointing to the ms2 spectrum.
      @param precursor Iterator pointing to the precursor spectrum of ms2_spec.
      @return Fraction of the total intensity in the isolation window of the precursor spectrum that was assigned to the precursor.
    */
    double computeSingleScanPrecursorPurity_(const MSExperiment<Peak1D>::ConstIterator& ms2_spec, const MSExperiment<Peak1D>::SpectrumType& precursor_spec) const;

protected:
    /// implemented for DefaultParamHandler
    void setDefaultParams_();

    /// implemented for DefaultParamHandler
    void updateMembers_();
  };
} // namespace

#endif // OPENMS_ANALYSIS_QUANTITATION_ISOBARICCHANNELEXTRACTOR_H
