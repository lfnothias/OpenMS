// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2013.
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
// $Maintainer: Lars Nilse $
// $Authors: Lars Nilse, Mathias Walzer $
// --------------------------------------------------------------------------

#include <OpenMS/config.h>
#include <OpenMS/APPLICATIONS/TOPPBase.h>
#include <OpenMS/CONCEPT/ProgressLogger.h>
#include <OpenMS/DATASTRUCTURES/DBoundingBox.h>
#include <OpenMS/DATASTRUCTURES/ListUtils.h>
#include <OpenMS/KERNEL/MSExperiment.h>
#include <OpenMS/KERNEL/StandardTypes.h>
#include <OpenMS/KERNEL/ConsensusMap.h>
#include <OpenMS/KERNEL/FeatureMap.h>
#include <OpenMS/FORMAT/MzMLFile.h>
#include <OpenMS/FORMAT/FeatureXMLFile.h>
#include <OpenMS/MATH/STATISTICS/LinearRegression.h>
#include <OpenMS/KERNEL/RangeUtils.h>
#include <OpenMS/KERNEL/ChromatogramTools.h>
#include <OpenMS/FORMAT/MzQuantMLFile.h>
#include <OpenMS/METADATA/MSQuantifications.h>
#include <OpenMS/TRANSFORMATIONS/RAW2PEAK/PeakPickerHiRes.h>
#include <OpenMS/MATH/STATISTICS/LinearRegression.h>

#include <OpenMS/FORMAT/FeatureXMLFile.h>
#include <OpenMS/FORMAT/ConsensusXMLFile.h>
#include <OpenMS/FORMAT/MzQuantMLFile.h>

#include <OpenMS/TRANSFORMATIONS/FEATUREFINDER/MultiplexFiltering.h>
#include <OpenMS/TRANSFORMATIONS/FEATUREFINDER/MultiplexClustering.h>
#include <OpenMS/COMPARISON/CLUSTERING/GridBasedCluster.h>
#include <OpenMS/DATASTRUCTURES/DPosition.h>
#include <OpenMS/DATASTRUCTURES/DBoundingBox.h>

//Contrib includes
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

#include <QDir>

//std includes
#include <cmath>
#include <vector>
#include <algorithm>
#include <fstream>
#include <limits>
#include <locale>
#include <iomanip>

using namespace std;
using namespace OpenMS;
using namespace boost::math;

//-------------------------------------------------------------
//Doxygen docu
//-------------------------------------------------------------

/**
  @page TOPP_FeatureFinderMultiplex FeatureFinderMultiplex

  @brief Identifies peptide pairs in LC-MS data and determines their relative abundance.

<CENTER>
  <table>
    <tr>
      <td ALIGN = "center" BGCOLOR="#EBEBEB"> pot. predecessor tools </td>
      <td VALIGN="middle" ROWSPAN=3> \f$ \longrightarrow \f$ FeatureFinderMultiplex \f$ \longrightarrow \f$</td>
      <td ALIGN = "center" BGCOLOR="#EBEBEB"> pot. successor tools </td>
    </tr>
    <tr>
      <td VALIGN="middle" ALIGN = "center" ROWSPAN=1> @ref TOPP_FileConverter </td>
      <td VALIGN="middle" ALIGN = "center" ROWSPAN=2> @ref TOPP_IDMapper</td>
    </tr>
    <tr>
      <td VALIGN="middle" ALIGN = "center" ROWSPAN=1> @ref TOPP_FileFilter </td>
    </tr>
  </table>
</CENTER>

  FeatureFinderMultiplex is a tool for the fully automated analysis of quantitative proteomics data. It identifies pairs of isotopic envelopes with fixed m/z separation. It requires no prior sequence identification of the peptides. In what follows we first explain the algorithm and then discuss the tuning of its parameters.

  <b>Algorithm</b>

  The algorithm is divided into three parts: filtering, clustering and linear fitting, see Fig. (d), (e) and (f). In the following discussion let us consider a particular mass spectrum at retention time 1350 s, see Fig. (a). It contains a peptide of mass 1492 Da and its 6 Da heavier labelled counterpart. Both are doubly charged in this instance. Their isotopic envelopes therefore appear at 746 and 749 in the spectrum. The isotopic peaks within each envelope are separated by 0.5. The spectrum was recorded at finite intervals. In order to read accurate intensities at arbitrary m/z we spline-fit over the data, see Fig. (b).

  We would like to search for such peptide pairs in our LC-MS data set. As a warm-up let us consider a standard intensity cut-off filter, see Fig. (c). Scanning through the entire m/z range (red dot) only data points with intensities above a certain threshold pass the filter. Unlike such a local filter, the filter used in our algorithm takes intensities at a range of m/z positions into account, see Fig. (d). A data point (red dot) passes if
  - all six intensities at m/z, m/z+0.5, m/z+1, m/z+3, m/z+3.5 and m/z+4 lie above a certain threshold,
  - the intensity profiles in neighbourhoods around all six m/z positions show a good correlation and
  - the relative intensity ratios within a peptide agree up to a factor with the ratios of a theoretic averagine model.

  Let us now filter not only a single spectrum but all spectra in our data set. Data points that pass the filter form clusters in the t-m/z plane, see Fig. (e). Each cluster corresponds to the mono-isotopic mass trace of the lightest peptide of a SILAC pattern. We now use hierarchical clustering methods to assign each data point to a specific cluster. The optimum number of clusters is determined by maximizing the silhouette width of the partitioning. Each data point in a cluster corresponds to three pairs of intensities (at [m/z, m/z+3], [m/z+0.5, m/z+3.5] and [m/z+1, m/z+4]). A plot of all intensity pairs in a cluster shows a clear linear correlation, see Fig. (f). Using linear regression we can determine the relative amounts of labelled and unlabelled peptides in the sample.

  @image html SILACAnalyzer_algorithm.png

  <B>The command line parameters of this tool are:</B>
  @verbinclude TOPP_FeatureFinderMultiplex.cli
    <B>INI file documentation of this tool:</B>
    @htmlinclude TOPP_FeatureFinderMultiplex.html

  <b>Parameter Tuning</b>

  FeatureFinderMultiplex can detect SILAC patterns of any number of peptides, i.e. doublets (pairs), triplets, quadruplets et cetera.

  <i>input:</i>
  - in [*.mzML] - LC-MS dataset to be analyzed
  - ini [*.ini] - file containing all parameters (see discussion below)

  <i>standard output:</i>
  - out [*.consensusXML] - contains the list of identified peptides (retention time and m/z of the lightest peptide, ratios)

  <i>optional output:</i>
  - out_clusters [*.consensusXML] - contains the complete set of data points passing the filters, see Fig. (e)

  The results of an analysis can easily visualized within TOPPView. Simply load *.consensusXML and *.featureXML as layers over the original *.mzML.

  Parameters in section <i>algorithm:</i>
  - <i>allow_missing_peaks</i> - Low intensity peaks might be missing from the isotopic pattern of some of the peptides. Specify if such peptides should be included in the analysis.
  - <i>rt_typical</i> - Upper bound for the retention time [s] over which a characteristic peptide elutes.
  - <i>rt_min</i> - Lower bound for the retentions time [s].
  - <i>intensity_cutoff</i> - Lower bound for the intensity of isotopic peaks in a SILAC pattern.
  - <i>peptide_similarity</i> - Lower bound for the Pearson correlation coefficient, which measures how well intensity profiles of different isotopic peaks correlate.
  - <i>averagine_similarity</i> - Upper bound on the factor by which the ratios of observed isotopic peaks are allowed to differ from the ratios of the theoretic averagine model, i.e. ( theoretic_ratio / model_deviation ) < observed_ratio < ( theoretic_ratio * model_deviation ).

  Parameters in section <i>algorithm:</i>
  - <i>labels</i> - Labels used for labelling the sample. [...] specifies the labels for a single sample. For example, [Lys4,Arg6][Lys8,Arg10] describes a mixtures of three samples. One of them unlabelled, one labelled with Lys4 and Arg6 and a third one with Lys8 and Arg10. For permitted labels see section <i>labels</i>.
  - <i>charge</i> - Range of charge states in the sample, i.e. min charge : max charge.
  - <i>missed_cleavages</i> - Maximum number of missed cleavages.
  - <i>isotopes_per_peptide</i> - Range of peaks per peptide in the sample, i.e. min peaks per peptide : max peaks per peptide.

 Parameters in section <i>labels:</i>
 This section contains a list of all isotopic labels currently available for analysis of SILAC data with FeatureFinderMultiplex.

 <b>References:</b>
  @n L. Nilse, M. Sturm, D. Trudgian, M. Salek, P. Sims, K. Carroll, S. Hubbard,  <a href="http://www.springerlink.com/content/u40057754100v71t">SILACAnalyzer - a tool for differential quantitation of stable isotope derived data</a>, in F. Masulli, L. Peterson, and R. Tagliaferri (Eds.): CIBB 2009, LNBI 6160, pp. 4555, 2010.
*/

// We do not want this class to show up in the docu:
/// @cond TOPPCLASSES

class TOPPFeatureFinderMultiplex :
  public TOPPBase
{
private:

  // input and output files
  String in_;
  String out_;
  String out_features_;
  String out_mzq_;
  String out_debug_;
  bool debug_;

  // section "algorithm"
  String labels_;
  unsigned charge_min_;
  unsigned charge_max_;
  unsigned missed_cleavages_;
  unsigned isotopes_per_peptide_min_;
  unsigned isotopes_per_peptide_max_;
  double rt_typical_;
  double rt_min_;
  double mz_tolerance_;
  bool mz_unit_;    // ppm (true), Da (false)
  double intensity_cutoff_;
  double peptide_similarity_;
  double averagine_similarity_;
  bool knock_out_;
  
  // section "labels"
  map<String,double> label_massshift;

public:
  TOPPFeatureFinderMultiplex() :
  TOPPBase("FeatureFinderMultiplex", "Determination of peak ratios in LC-MS data", true)
  {
  }
  
  typedef std::vector<double> MassPattern;    // list of mass shifts

  void registerOptionsAndFlags_()
  {
    registerInputFile_("in", "<file>", "", "Raw LC-MS data to be analyzed. (Profile data required. Will not work with centroided data!)");
    setValidFormats_("in", ListUtils::create<String>("mzML"));
    registerOutputFile_("out", "<file>", "", "Set of all identified peptide groups (i.e. peptide pairs or triplets or singlets or ..). The m/z-RT positions correspond to the lightest peptide in each group.", false);
    setValidFormats_("out", ListUtils::create<String>("consensusXML"));
    registerOutputFile_("out_features", "<file>", "", "Optional output file containing the individual peptide features in \'out\'.", false, true);
    setValidFormats_("out_features", ListUtils::create<String>("featureXML"));
    registerOutputFile_("out_mzq", "<file>", "", "Optional output file of MzQuantML.", false, true);
    setValidFormats_("out_mzq", ListUtils::create<String>("mzq"));
    registerStringOption_("out_debug", "<out_dir>", "", "Directory for debug output.", false, true);

    registerSubsection_("algorithm", "Parameters for the algorithm.");
    registerSubsection_("labels", "Isotopic labels that can be specified in section \'algorithm:labels\'.");    
    
  }

  // create prameters for sections (set default values and restrictions)
  Param getSubsectionDefaults_(const String & section) const
  {
    Param defaults;

    if (section == "algorithm")
    {
      defaults.setValue("labels", "[][Lys8,Arg10]", "Labels used for labelling the samples. [...] specifies the labels for a single sample. For example\n\n[][Lys8,Arg10]        ... SILAC\n[][Lys4,Arg6][Lys8,Arg10]        ... triple-SILAC\n[Dimethyl0][Dimethyl6]        ... Dimethyl\n[Dimethyl0][Dimethyl4][Dimethyl8]        ... triple Dimethyl\n[ICPL0][ICPL4][ICPL6][ICPL10]        ... ICPL");
      defaults.setValue("charge", "1:4", "Range of charge states in the sample, i.e. min charge : max charge.");
      defaults.setValue("isotopes_per_peptide", "3:6", "Range of isotopes per peptide in the sample. For example 3:6, if isotopic peptide patterns in the sample consist of either three, four, five or six isotopic peaks. ", ListUtils::create<String>("advanced"));
      defaults.setValue("rt_typical", 90.0, "Typical retention time [s] over which a characteristic peptide elutes. (This is not an upper bound. Peptides that elute for longer will be reported.)");
      defaults.setMinFloat("rt_typical", 0.0);
      defaults.setValue("rt_min", 5.0, "Lower bound for the retention time [s]. (Any peptides seen for a shorter time period are not reported.)");
      defaults.setMinFloat("rt_min", 0.0);
      defaults.setValue("mz_tolerance", 6.0, "m/z tolerance for search of peak patterns.");
      defaults.setMinFloat("mz_tolerance", 0.0);
      defaults.setValue("mz_unit", "ppm", "Unit of the 'mz_tolerance' parameter.");
      defaults.setValidStrings("mz_unit", ListUtils::create<String>("Da,ppm"));
      defaults.setValue("intensity_cutoff", 1000.0, "Lower bound for the intensity of isotopic peaks.");
      defaults.setMinFloat("intensity_cutoff", 0.0);
      defaults.setValue("peptide_similarity", 0.7, "Two peptides in a multiplet are expected to have the same isotopic pattern. This parameter is a lower bound on their similarity.");
      defaults.setMinFloat("peptide_similarity", 0.0);
      defaults.setMaxFloat("peptide_similarity", 1.0);
      defaults.setValue("averagine_similarity", 0.6, "The isotopic pattern of a peptide should resemble the averagine model at this m/z position. This parameter is a lower bound on similarity between measured isotopic pattern and the averagine model.");
      defaults.setMinFloat("averagine_similarity", 0.0);
      defaults.setMaxFloat("averagine_similarity", 1.0);
      defaults.setValue("missed_cleavages", 0, "Maximum number of missed cleavages due to incomplete digestion.");
      defaults.setMinInt("missed_cleavages", 0);
      defaults.setValue("knock_out", "true", "Is it likely that knock-outs are present?", ListUtils::create<String>("advanced"));
      defaults.setValidStrings("knock_out", ListUtils::create<String>("true,false"));
    }

    if (section == "labels")
    {
      // create labels that can be chosen in section "algorithm/labels"
      defaults.setValue("Arg6", 6.0201290268, "Label:13C(6)  |  C(-6) 13C(6)  |  unimod #188", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("Arg6", 0.0);
      defaults.setValue("Arg10", 10.008268600, "Label:13C(6)15N(4)  |  C(-6) 13C(6) N(-4) 15N(4)  |  unimod #267", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("Arg10", 0.0);
      defaults.setValue("Lys4", 4.0251069836, "Label:2H(4)  |  H(-4) 2H(4)  |  unimod #481", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("Lys4", 0.0);
      defaults.setValue("Lys6", 6.0201290268, "Label:13C(6)  |  C(-6) 13C(6)  |  unimod #188", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("Lys6", 0.0);
      defaults.setValue("Lys8", 8.0141988132, "Label:13C(6)15N(2)  |  C(-6) 13C(6) N(-2) 15N(2)  |  unimod #259", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("Lys8", 0.0);
      defaults.setValue("Dimethyl0", 28.031300, "Dimethyl  |  H(4) C(2)  |  unimod #36", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("Dimethyl0", 0.0);
      defaults.setValue("Dimethyl4", 32.056407, "Dimethyl:2H(4)  |  2H(4) C(2)  |  unimod #199", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("Dimethyl4", 0.0);
      defaults.setValue("Dimethyl6", 34.063117, "Dimethyl:2H(4)13C(2)  |  2H(4) 13C(2)  |  unimod #510", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("Dimethyl6", 0.0);
      defaults.setValue("Dimethyl8", 36.075670, "Dimethyl:2H(6)13C(2)  |  H(-2) 2H(6) 13C(2)  |  unimod #330", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("Dimethyl8", 0.0);
      defaults.setValue("ICPL0", 105.021464, "ICPL  |  H(3) C(6) N O  |  unimod #365", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("ICPL0", 0.0);
      defaults.setValue("ICPL4", 109.046571, "ICPL:2H(4)  |  H(-1) 2H(4) C(6) N O  |  unimod #687", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("ICPL4", 0.0);
      defaults.setValue("ICPL6", 111.041593, "ICPL:13C(6)  |  H(3) 13C(6) N O  |  unimod #364", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("ICPL6", 0.0);
      defaults.setValue("ICPL10", 115.066700, "ICPL:13C(6)2H(4)  |  H(-1) 2H(4) 13C(6) N O  |  unimod #866", ListUtils::create<String>("advanced"));
      defaults.setMinFloat("ICPL10", 0.0);
      //defaults.setValue("18O", 2.004246, "Label:18O(1)  |  O(-1) 18O  |  unimod #258", ListUtils::create<String>("advanced"));
      //defaults.setMinFloat("18O", 0.0);
    }

    return defaults;
  }

  void getParameters_in_out_()
  {
    in_ = getStringOption_("in");
    out_ = getStringOption_("out");
    out_features_ = getStringOption_("out_features");
    out_mzq_ = getStringOption_("out_mzq");
    out_debug_ = getStringOption_("out_debug");
    debug_ = !out_debug_.empty();
  }

  void getParameters_algorithm_()
  {
    // get selected labels
    labels_ = getParam_().getValue("algorithm:labels");

    // get selected charge range
    String charge_string = getParam_().getValue("algorithm:charge");
    double charge_min_temp, charge_max_temp;
    parseRange_(charge_string, charge_min_temp, charge_max_temp);
    charge_min_ = charge_min_temp;
    charge_max_ = charge_max_temp;
    if (charge_min_ > charge_max_)
    {
      swap(charge_min_, charge_max_);
    }

    // get isotopes per peptide range
    String isotopes_per_peptide_string = getParam_().getValue("algorithm:isotopes_per_peptide");
    double isotopes_per_peptide_min_temp, isotopes_per_peptide_max_temp;
    parseRange_(isotopes_per_peptide_string, isotopes_per_peptide_min_temp, isotopes_per_peptide_max_temp);
    isotopes_per_peptide_min_ = isotopes_per_peptide_min_temp;
    isotopes_per_peptide_max_ = isotopes_per_peptide_max_temp;
    if (isotopes_per_peptide_min_ > isotopes_per_peptide_max_)
    {
      swap(isotopes_per_peptide_min_, isotopes_per_peptide_max_);
    }

    rt_typical_ = getParam_().getValue("algorithm:rt_typical");
    rt_min_ = getParam_().getValue("algorithm:rt_min");
    mz_tolerance_ = getParam_().getValue("algorithm:mz_tolerance");
    mz_unit_ = (getParam_().getValue("algorithm:mz_unit") == "ppm");
    intensity_cutoff_ = getParam_().getValue("algorithm:intensity_cutoff");
    peptide_similarity_ = getParam_().getValue("algorithm:peptide_similarity");
    averagine_similarity_ = getParam_().getValue("algorithm:averagine_similarity");
    missed_cleavages_ = getParam_().getValue("algorithm:missed_cleavages");
    knock_out_ = (getParam_().getValue("algorithm:knock_out") == "true");
  }

  void getParameters_labels_()
  {
    // create map of pairs (label as string, mass shift as double)
    label_massshift.insert(make_pair("Arg6", getParam_().getValue("labels:Arg6")));
    label_massshift.insert(make_pair("Arg10", getParam_().getValue("labels:Arg10")));
    label_massshift.insert(make_pair("Lys4", getParam_().getValue("labels:Lys4")));
    label_massshift.insert(make_pair("Lys6", getParam_().getValue("labels:Lys6")));
    label_massshift.insert(make_pair("Lys8", getParam_().getValue("labels:Lys8")));
    label_massshift.insert(make_pair("Dimethyl0", getParam_().getValue("labels:Dimethyl0")));
    label_massshift.insert(make_pair("Dimethyl4", getParam_().getValue("labels:Dimethyl4")));
    label_massshift.insert(make_pair("Dimethyl6", getParam_().getValue("labels:Dimethyl6")));
    label_massshift.insert(make_pair("Dimethyl8", getParam_().getValue("labels:Dimethyl8")));
    label_massshift.insert(make_pair("ICPL0", getParam_().getValue("labels:ICPL0")));
    label_massshift.insert(make_pair("ICPL4", getParam_().getValue("labels:ICPL4")));
    label_massshift.insert(make_pair("ICPL6", getParam_().getValue("labels:ICPL6")));
    label_massshift.insert(make_pair("ICPL10", getParam_().getValue("labels:ICPL10")));
  }
    
	// generate list of mass patterns
	std::vector<MassPattern> generateMassPatterns_()
	{
        // SILAC, Dimethyl, ICPL or no labelling ??
        
        bool labelling_SILAC = ((labels_.find("Arg")!=std::string::npos) || (labels_.find("Lys")!=std::string::npos));
        bool labelling_Dimethyl = (labels_.find("Dimethyl")!=std::string::npos);
        bool labelling_ICPL = (labels_.find("ICPL")!=std::string::npos);
        bool labelling_none = labels_.empty() || (labels_=="[]") || (labels_=="()") || (labels_=="{}");
        
        bool SILAC = (labelling_SILAC && !labelling_Dimethyl && !labelling_ICPL && !labelling_none);
        bool Dimethyl = (!labelling_SILAC && labelling_Dimethyl && !labelling_ICPL && !labelling_none);
        bool ICPL = (!labelling_SILAC && !labelling_Dimethyl && labelling_ICPL && !labelling_none);
        bool none = (!labelling_SILAC && !labelling_Dimethyl && !labelling_ICPL && labelling_none);
        
        if (!(SILAC || Dimethyl || ICPL || none))
        {
            throw Exception::IllegalArgument(__FILE__, __LINE__, __PRETTY_FUNCTION__, "Unknown labelling. Neither SILAC, Dimethyl nor ICPL.");
        }    
        
        // split labels string
        std::vector<std::vector<String> > samples_labels;    // list of samples containing lists of corresponding labels
        std::vector<String> temp_samples;
        boost::split(temp_samples, labels_, boost::is_any_of("[](){}"));   // any bracket allowed to separate samples
        for (unsigned i = 0; i < temp_samples.size(); ++i)
        {
          if (!temp_samples[i].empty())
          {
              vector<String> temp_labels;
              boost::split(temp_labels, temp_samples[i], boost::is_any_of(",;: "));   // various separators allowed to separate labels
              samples_labels.push_back(temp_labels);
          }
        }

        // debug output labels
        cout << "\n";
        for (unsigned i = 0; i < samples_labels.size(); ++i)
        {
          cout << "sample " << (i + 1) << ":   ";
          for (unsigned j = 0; j < samples_labels[i].size(); ++j)
          {
            cout << samples_labels[i][j] << " ";
          }
          cout << "\n";
        }
        
        // check if the labels are included in advanced section "labels"
        String all_labels= "Arg6 Arg10 Lys4 Lys6 Lys8 Dimethyl0 Dimethyl4 Dimethyl6 Dimethyl8 ICPL0 ICPL4 ICPL6 ICPL10";
        for (unsigned i = 0; i < samples_labels.size(); i++)
        {
          for (unsigned j = 0; j < samples_labels[i].size(); ++j)
          {
            if (all_labels.find(samples_labels[i][j])==std::string::npos)
            {
              std::stringstream stream;
              stream << "The label " << samples_labels[i][j] << " is unknown.";
              throw Exception::InvalidParameter(__FILE__, __LINE__, __PRETTY_FUNCTION__, stream.str());
            }
          }
        }
        
        // generate mass shift list
        std::vector<MassPattern> list;
        if (SILAC)
        {
            // SILAC
            // We assume the first sample to be unlabelled. Even if the "[]" for the first sample in the label string has not been specified.
            
            for (unsigned ArgPerPeptide = 0; ArgPerPeptide <= missed_cleavages_ + 1; ArgPerPeptide++)
            {
                for (unsigned LysPerPeptide = 0; LysPerPeptide <= missed_cleavages_ + 1; LysPerPeptide++)
                {
                    if (ArgPerPeptide + LysPerPeptide <= missed_cleavages_ + 1)
                    {
                        MassPattern temp;
                        temp.push_back(0);
                        for (unsigned i = 0; i < samples_labels.size(); i++)
                        {
                            double mass_shift = 0;
                            // Considering the case of an amino acid (e.g. LysPerPeptide != 0) for which no label is present (e.g. Lys4There + Lys8There == 0) makes no sense. Therefore each amino acid will have to give its "Go Ahead" before the shift is calculated.
                            bool goAhead_Lys = false;
                            bool goAhead_Arg = false;
                            
                            for (unsigned j = 0; j < samples_labels[i].size(); ++j)
                            {
                                int Arg6There = 0;    // Is Arg6 in the SILAC label?
                                int Arg10There = 0;
                                int Lys4There = 0;
                                int Lys6There = 0;
                                int Lys8There = 0;
                                
                                if (samples_labels[i][j].find("Arg6")!=std::string::npos) Arg6There = 1;
                                if (samples_labels[i][j].find("Arg10")!=std::string::npos) Arg10There = 1;
                                if (samples_labels[i][j].find("Lys4")!=std::string::npos) Lys4There = 1;
                                if (samples_labels[i][j].find("Lys6")!=std::string::npos) Lys6There = 1;
                                if (samples_labels[i][j].find("Lys8")!=std::string::npos) Lys8There = 1;
                                
                                mass_shift = mass_shift + ArgPerPeptide * (Arg6There * label_massshift["Arg6"] + Arg10There * label_massshift["Arg10"]) + LysPerPeptide * (Lys4There * label_massshift["Lys4"] + Lys6There * label_massshift["Lys6"] + Lys8There * label_massshift["Lys8"]);

                                // check that Arg (or Lys) is in the peptide and label
                                goAhead_Arg = goAhead_Arg || !((ArgPerPeptide != 0 && Arg6There + Arg10There == 0));
                                goAhead_Lys = goAhead_Lys || !((LysPerPeptide != 0 && Lys4There + Lys6There + Lys8There == 0));
                            }
                            
                            if (goAhead_Arg && goAhead_Lys && (mass_shift!=0))
                            {
                                temp.push_back(mass_shift);
                            }
                        }
                        
                        if (temp.size()>1)
                        {
                            list.push_back(temp);
                        }                        
                    }
                }
            }
            
        }
        else if (Dimethyl || ICPL)
        {
            // Dimethyl or ICPL
            // We assume each sample to be labelled only once.
            
            for (unsigned mc = 0; mc <= missed_cleavages_; ++mc)
            {
                MassPattern temp;
                for (unsigned i = 0; i < samples_labels.size(); i++)
                {
                    temp.push_back((mc+1)*(label_massshift[samples_labels[i][0]] - label_massshift[samples_labels[0][0]]));
                }
                list.push_back(temp);
            }
            
        }
        else
        {
            // none (singlet detection)
            MassPattern temp;
            temp.push_back(0);
            list.push_back(temp);
        }
        
        // generate all mass shifts that can occur due to the absence of one or multiple peptides 
        // (e.g. for a triplet experiment generate the doublets and singlets that might be present)
        unsigned n = list[0].size();    // n=2 for doublets, n=3 for triplets ...
        if (knock_out_ && n==4)
        {
             unsigned m = list.size();
             for (unsigned i = 0; i < m; ++i)
             {
                 MassPattern triplet1(1,0);
                 triplet1.push_back(list[i][2]-list[i][1]);
                 triplet1.push_back(list[i][3]-list[i][1]);
                 list.push_back(triplet1);
                 
                 MassPattern triplet2(1,0);
                 triplet2.push_back(list[i][2]-list[i][0]);
                 triplet2.push_back(list[i][3]-list[i][0]);
                 list.push_back(triplet2);
                 
                 MassPattern triplet3(1,0);
                 triplet3.push_back(list[i][1]-list[i][0]);
                 triplet3.push_back(list[i][2]-list[i][0]);
                 list.push_back(triplet3);
                 
    
                 MassPattern doublet1(1,0);
                 doublet1.push_back(list[i][1]);
                 list.push_back(doublet1);
                 
                 MassPattern doublet2(1,0);
                 doublet2.push_back(list[i][2]);
                 list.push_back(doublet2);
                 
                 MassPattern doublet3(1,0);
                 doublet3.push_back(list[i][3]);
                 list.push_back(doublet3);
                 
                 MassPattern doublet4(1,0);
                 doublet4.push_back(list[i][2] - list[i][1]);
                 list.push_back(doublet4);
                 
                 MassPattern doublet5(1,0);
                 doublet5.push_back(list[i][3]-list[i][1]);
                 list.push_back(doublet5);
                 
                 MassPattern doublet6(1,0);
                 doublet6.push_back(list[i][3] - list[i][2]);
                 list.push_back(doublet6);
             }
             
             MassPattern singlet(1,0);
             list.push_back(singlet);
        }
        else if (knock_out_ && n==3)
        {
             unsigned m = list.size();
             for (unsigned i = 0; i < m; ++i)
             {
                 MassPattern doublet1(1,0);
                 doublet1.push_back(list[i][1]);
                 list.push_back(doublet1);
                 
                 MassPattern doublet2(1,0);
                 doublet2.push_back(list[i][2]-list[i][1]);
                 list.push_back(doublet2);
                 
                 MassPattern doublet3(1,0);
                 doublet3.push_back(list[i][2]);
                 list.push_back(doublet3);
             }
             
             MassPattern singlet(1,0);
             list.push_back(singlet);
        }
        else if (knock_out_ && n==2)
        {
             MassPattern singlet(1,0);
             list.push_back(singlet);
        }
        
        // debug output mass shifts
        cout << "\n";
        for (unsigned i = 0; i < list.size(); ++i)
        {
            std::cout << "mass shift " << (i+1) << ":    ";
            MassPattern temp = list[i];
            for (unsigned j = 0; j < temp.size(); ++j)
            {
                std::cout << temp[j] << "  ";
            }
            std::cout << "\n";
        }
        std::cout << "\n";
        
		return list;
	}
  
	// generate list of mass shifts
	std::vector<MultiplexPeakPattern> generatePeakPatterns_(int charge_min, int charge_max, int peaksPerPeptideMax, std::vector<MassPattern> massPatternList)
	{
		std::vector<MultiplexPeakPattern> list;
	  
		// iterate over all charge states (from max to min)
		// 4+ can be mistaken as 2+, but 2+ not as 4+
		for (int c = charge_max; c >= charge_min; --c)
		{
			// iterate over all mass shifts (from small to large shifts)
			// first look for the more likely non-missed-cleavage cases
			// e.g. first (0, 4, 8) then (0, 8, 16)
			for (unsigned i = 0; i < massPatternList.size(); ++i)
			{
				MultiplexPeakPattern pattern(c, peaksPerPeptideMax, massPatternList[i], i);
				list.push_back(pattern);
			}
		} 
        
		return list;
	}
    
    /**
     * @brief calculate peptide intensities 
     * 
     * @param profile_intensities    vectors of profile intensities for each of the peptides (first index: peptide 0=L, 1=M, 2=H etc, second index: raw data point)
     * @return vector with intensities for each peptide
     */
    std::vector<double> getPeptideIntensities(std::vector<std::vector<double> > &profile_intensities)
    {
        unsigned count = profile_intensities[0].size();
        for (unsigned i = 0; i < profile_intensities.size(); ++i)
        {
            if (profile_intensities[i].size() != count)
            {
                throw Exception::IllegalArgument(__FILE__, __LINE__, __PRETTY_FUNCTION__, "The profile intensity vectors for each peptide are not of the same size.");
            }
        }
        
        // determine ratios through linear regression
        std::vector<double> ratios;    // L:L, M:L, H:L etc.
        std::vector<double> intensities;    // L, M, H etc.
        // loop over peptides
        for (unsigned i = 0; i < profile_intensities.size(); ++i)
        {
            // filter for non-NaN intensities
            std::vector<double> intensities1;
            std::vector<double> intensities2;
            double intensity = 0;
            for (unsigned j = 0; j < count; ++j)
            {
                if (!(boost::math::isnan(profile_intensities[0][j])) && !(boost::math::isnan(profile_intensities[i][j])))
                {
                    intensities1.push_back(profile_intensities[0][j]);
                    intensities2.push_back(profile_intensities[i][j]);
                    intensity += profile_intensities[i][j];
                }
            }
            
            LinearRegressionWithoutIntercept linreg;
            linreg.addData(intensities1, intensities2);
            ratios.push_back(linreg.getSlope());          
            intensities.push_back(intensity);

        }
        
        // correct peptide intensities
        // (so that their ratios are in agreement with the ratios from the linear regression)
        std::vector<double> corrected_intensities;
        if (profile_intensities.size() == 2)
        {
            double intensity1 = (intensities[0] + ratios[1]*intensities[1])/(1+ratios[1]*ratios[1]);
            double intensity2 = ratios[1] * intensity1;
            std::cout << "x = " << intensities[0] << "  y = " << intensities[1] << "  y/x = " << intensities[1]/intensities[0] << "  ratio = " << ratios[1] << "  x' = " << intensity1 << "  y' = " << intensity2 << "\n";
            corrected_intensities.push_back(intensity1);
            corrected_intensities.push_back(intensity2);
        }
        else if (profile_intensities.size() > 2)
        {
            // Now there are multiple labelled/light ratios. Light intensities stay fixed, just adjust intensities of labelled peptides.
            // TODO: Better project onto hyperplane defined by ratios, then light is not singled out.
            corrected_intensities.push_back(intensities[0]);
            for (unsigned i = 1; i < profile_intensities.size(); ++i)
            {
                corrected_intensities.push_back(ratios[i] * intensities[0]);
            }
        }
        else
        {
            corrected_intensities.push_back(intensities[0]);
        }
        
        return corrected_intensities;
    }
    
    /**
     * @brief generates consensus and feature maps containing all peptide multiplets
     * 
     * @param patterns    patterns of isotopic peaks we have been searching for
     * @param filter_results    filter results for each of the patterns
     * @param cluster_results    clusters of filter results
     * @param consensus_map    consensus map with peptide multiplets (to be filled)
     * @param feature_map    feature map with peptides (to be filled)
     */
     void generateMaps_(std::vector<MultiplexPeakPattern> patterns, std::vector<MultiplexFilterResult> filter_results, std::vector<std::map<int,GridBasedCluster> > cluster_results, ConsensusMap &consensus_map, FeatureMap<> &feature_map)
    {
        // data structures for final results
        std::vector<std::vector<double> > peptide_RT;
        std::vector<std::vector<double> > peptide_MZ;
        std::vector<std::vector<double> > peptide_intensity;
        std::vector<std::vector<int> > peptide_charge;

        // loop over peak patterns
        for (unsigned pattern = 0; pattern < patterns.size(); ++pattern)
        {
            std::cout << "\n" << "pattern " << pattern << " contains " << cluster_results[pattern].size() << " clusters.\n";
            
            // loop over clusters
            for (std::map<int,GridBasedCluster>::const_iterator cluster_it = cluster_results[pattern].begin(); cluster_it != cluster_results[pattern].end(); ++cluster_it)
            {
                ConsensusFeature consensus;
                
                // The position (m/z, RT) of the peptide features is the centre-of-mass of the mass trace of the lightest isotope.
                // The centre-of-mass is the intensity-weighted average of the peak positions.
                std::vector<double> sum_intensity_mz;
                std::vector<double> sum_intensity_rt;
                std::vector<double> sum_intensity;
                for (unsigned peptide = 0; peptide < patterns[pattern].getMassShiftCount(); ++peptide)
                {
                    sum_intensity_mz.push_back(0);
                    sum_intensity_rt.push_back(0);
                    sum_intensity.push_back(0);
                }
                // (Spline-interpolated) profile intensities for accurate ratio determination.
                // First index is the peptide, second is just the profile intensities collected
                std::vector<std::vector<double> > profile_intensities;
                for (unsigned peptide = 0; peptide < patterns[pattern].getMassShiftCount(); ++peptide)
                {
                    std::vector<double> temp;
                    profile_intensities.push_back(temp);
                }
                
                GridBasedCluster cluster = cluster_it->second;
                std::vector<int> points = cluster.getPoints();
                std::cout << "  The cluster contains " << points.size() << " points.\n";
                
                // loop over points in cluster
                for (std::vector<int>::const_iterator point_it = points.begin(); point_it != points.end(); ++point_it)
                {
                    int index = (*point_it);
                    //double RT = filter_results[pattern].getRT(index);
                    //std::cout << "    index = " << index << "  RT = " << RT << "\n";
                    
                    MultiplexFilterResultPeak result_peak = filter_results[pattern].getFilterResultPeak(index);
                    
                    for (unsigned peptide = 0; peptide < patterns[pattern].getMassShiftCount(); ++peptide)
                    {
                        sum_intensity_mz[peptide] += (result_peak.getMZ() + result_peak.getMZShifts()[(isotopes_per_peptide_max_ +1)*peptide + 1]) * result_peak.getIntensities()[(isotopes_per_peptide_max_ +1)*peptide + 1];
                        sum_intensity_rt[peptide] += result_peak.getRT() * result_peak.getIntensities()[(isotopes_per_peptide_max_ +1)*peptide + 1];
                        sum_intensity[peptide]  += result_peak.getIntensities()[(isotopes_per_peptide_max_ +1)*peptide + 1];
                    }
                    
                    // iterate over profile data
                    // (We use the (spline-interpolated) profile intensities for a very accurate ratio determination.)
                    for (int i = 0; i < result_peak.size(); ++i)
                    {
                        MultiplexFilterResultRaw result_raw = result_peak.getFilterResultRaw(i);
                        
                        // loop over isotopic peaks in peptide
                        for (unsigned peak = 0; peak < isotopes_per_peptide_max_; ++peak)
                        {
                            // loop over peptides
                            for (unsigned peptide = 0; peptide < patterns[pattern].getMassShiftCount(); ++peptide)
                            {
                                profile_intensities[peptide].push_back(result_raw.getIntensities()[(isotopes_per_peptide_max_ + 1)*peptide + peak + 1]);    // +1 due to zeroth peaks
                            }
                        }
                    }
                }
                
                // calculate intensities for each of the peptides from profile data
                std::vector<double> peptide_intensities = getPeptideIntensities(profile_intensities);
                
                // average peptide intensity (= consensus intensity)
                double average_peptide_intensity = 0;
                for (unsigned i=0; i<peptide_intensities.size(); ++i)
                {
                    average_peptide_intensity += peptide_intensities[i];
                }
                average_peptide_intensity /= peptide_intensities.size();
                
                // fill map with consensuses and its features
                consensus.setMZ(sum_intensity_mz[0]/sum_intensity[0]);
                consensus.setRT(sum_intensity_rt[0]/sum_intensity[0]);
                consensus.setIntensity(average_peptide_intensity);
                consensus.setCharge(patterns[pattern].getCharge());
                consensus.setQuality(1 - 1/points.size());    // rough quality score in [0,1]
                
                for (unsigned peptide = 0; peptide < patterns[pattern].getMassShiftCount(); ++peptide)
                {
                    FeatureHandle feature_handle;
                    feature_handle.setMZ(sum_intensity_mz[peptide]/sum_intensity[peptide]);
                    feature_handle.setRT(sum_intensity_rt[peptide]/sum_intensity[peptide]);
                    feature_handle.setIntensity(peptide_intensities[peptide]);
                    feature_handle.setCharge(patterns[pattern].getCharge());
                    feature_handle.setMapIndex(peptide);
                    //feature_handle.setUniqueId(&UniqueIdInterface::setUniqueId);    // TODO: Do we need to set unique ID?
                    consensus_map.getFileDescriptions()[peptide].size++;
                    consensus.insert(feature_handle);
                    
                    Feature feature;
                    feature.setMZ(sum_intensity_mz[peptide]/sum_intensity[peptide]);
                    feature.setRT(sum_intensity_rt[peptide]/sum_intensity[peptide]);
                    feature.setIntensity(peptide_intensities[peptide]);
                    feature.setCharge(patterns[pattern].getCharge());
                    feature.setOverallQuality(1 - 1/points.size());
                    feature_map.push_back(feature);
                }
                
                consensus_map.push_back(consensus);
            }
                        
        }
        
    }
    
    /**
     * @brief Write consensus map to consensusXML file.
     * 
     * @param filename    name of consensusXML file
     * @param map    consensus map for output
     */
    void writeConsensusMap_(const String &filename, ConsensusMap &map) const
    {
        map.sortByPosition();
        map.applyMemberFunction(&UniqueIdInterface::setUniqueId);
        map.setExperimentType("multiplex");    // TO DO: adjust for SILAC, Dimethyl, ICPL etc.
        
        ConsensusMap::FileDescription& desc = map.getFileDescriptions()[0];
        desc.filename = filename;
        desc.label = "multiplex";
        
        ConsensusXMLFile file;
        file.store(filename, map);
    }

    /**
     * @brief Write feature map to featureXML file.
     * 
     * @param filename    name of featureXML file
     * @param map    feature map for output
     */
    void writeFeatureMap_(const String &filename, FeatureMap<> &map) const
    {
        map.sortByPosition();
        map.applyMemberFunction(&UniqueIdInterface::setUniqueId);

        FeatureXMLFile file;
        file.store(filename, map);
    }

    /**
    * @brief simple linear regression through the origin
    * 
    * TODO: combine with OpenMS/MATH/STATISTICS/LinearRegression
    */
    class OPENMS_DLLAPI LinearRegressionWithoutIntercept
    {
        public:
        /**
         * @brief constructor
         */
        LinearRegressionWithoutIntercept()
        : sum_xx_(0), sum_xy_(0), n_(0)
        {
        }
        
        /**
         * @brief adds an observation (x,y) to the regression data set.
         * 
         * @param x    independent variable value
         * @param y    dependent variable value
         */
        void addData(double x, double y)
        {
            sum_xx_ += x*x;
            sum_xy_ += x*y;
            
            ++n_;
        }
        
        /**
         * @brief adds observations (x,y) to the regression data set.
         * 
         * @param x    vector of independent variable values
         * @param y    vector of dependent variable values
         */
        void addData(std::vector<double> &x, std::vector<double> &y)
        {
            for (unsigned i=0; i<x.size(); ++i)
            {
                addData(x[i],y[i]);
            }
        }
        
        /**
         * @brief returns the slope of the estimated regression line.
         */
        double getSlope()
        {
            if (n_ < 2)
            {
                return std::numeric_limits<double>::quiet_NaN();    // not enough data
            }
            return sum_xy_/sum_xx_;
        }
        
        private:
        /**
         * @brief total variation in x
         */
        double sum_xx_;
    
        /**
         * @brief sum of products
         */
        double sum_xy_;
    
        /**
         * @brief number of observations
         */
        int n_;
        
    };


  ExitCodes main_(int, const char **)
  {
      
    /**
     * handle parameters
     */
    getParameters_in_out_();
    getParameters_labels_();
    getParameters_algorithm_();

    /**
     * load input
     */
    MzMLFile file;
    MSExperiment<Peak1D> exp;

    // only read MS1 spectra ...
    /*
    std::vector<int> levels;
    levels.push_back(1);
    file.getOptions().setMSLevels(levels);
    */
    
    LOG_DEBUG << "Loading input..." << endl;
    file.setLogType(log_type_);
    file.load(in_, exp);

    // update m/z and RT ranges
    exp.updateRanges();

    // extract level 1 spectra
    exp.getSpectra().erase(remove_if(exp.begin(), exp.end(), InMSLevelRange<MSExperiment<Peak1D>::SpectrumType>(ListUtils::create<Int>("1"), true)), exp.end());

    // sort according to RT and MZ
    exp.sortSpectra();

    /**
     * pick peaks
     */
	PeakPickerHiRes picker;
	Param param = picker.getParameters();
    param.setValue("ms1_only", DataValue("true"));
    param.setValue("signal_to_noise", 0.0);    // signal-to-noise estimation switched off
    picker.setParameters(param);
        
    std::vector<std::vector<PeakPickerHiRes::PeakBoundary> > boundaries_exp_s;    // peak boundaries for spectra
    std::vector<std::vector<PeakPickerHiRes::PeakBoundary> > boundaries_exp_c;    // peak boundaries for chromatograms

	MSExperiment<Peak1D> exp_picked;
    picker.pickExperiment(exp, exp_picked, boundaries_exp_s, boundaries_exp_c);	

    /**
     * filter for peak patterns
     */
    bool missing_peaks_ = false;
	std::vector<MassPattern> masses = generateMassPatterns_();
	std::vector<MultiplexPeakPattern> patterns = generatePeakPatterns_(charge_min_, charge_max_, isotopes_per_peptide_max_, masses);
    MultiplexFiltering filtering(exp, exp_picked, boundaries_exp_s, patterns, isotopes_per_peptide_min_, isotopes_per_peptide_max_, missing_peaks_, intensity_cutoff_, mz_tolerance_, mz_unit_, peptide_similarity_, averagine_similarity_, out_debug_);
    std::vector<MultiplexFilterResult> filter_results = filtering.filter();
     
    /**
     * cluster filter results
     */
    MultiplexClustering clustering(exp, exp_picked, boundaries_exp_s, rt_typical_, rt_min_, out_debug_);
    std::vector<std::map<int,GridBasedCluster> > cluster_results = clustering.cluster(filter_results);

    /**
     * write to output
     */
    ConsensusMap consensus_map;   
    FeatureMap<> feature_map;   
    generateMaps_(patterns, filter_results, cluster_results, consensus_map, feature_map);
    writeConsensusMap_(out_, consensus_map);
    writeFeatureMap_(out_features_, feature_map);
    
    std::cout << "The map contains " << consensus_map.size() << " consensuses.\n";






    // data to be passed through the algorithm
    /*vector<vector<SILACPattern> > data;
    MSQuantifications msq;
    vector<Clustering *> cluster_data;*/

    /*if (labels.empty() && !out.empty()) // incompatible parameters
    {
      writeLog_("Error: The 'out' parameter cannot be used without a label (parameter 'sample:labels'). Use 'out_features' instead.");
      return ILLEGAL_PARAMETERS;
    }*/

    // 
    // Initializing the SILACAnalzer with our parameters
    // 
    /*SILACAnalyzer analyzer;
    analyzer.setLogType(log_type_);
    analyzer.initialize(
      // section "sample"
      labels,
      charge_min,
      charge_max,
      missed_cleavages,
      isotopes_per_peptide_min,
      isotopes_per_peptide_max,
      // section "algorithm"
      rt_typical,
      rt_min,
      intensity_cutoff,
      peptide_similarity,
      averagine_similarity,
      allow_missing_peaks,
      // labels
      label_massshift);*/

    /*if (out_mzq_ != "")
    {
      vector<vector<String> > SILAClabels = analyzer.getSILAClabels(); // list of SILAC labels, e.g. labels="[Lys4,Arg6][Lys8,Arg10]" => SILAClabels[0][1]="Arg6"

      std::vector<std::vector<std::pair<String, DoubleReal> > > labels;
      //add none label
      labels.push_back(std::vector<std::pair<String, DoubleReal> >(1, std::make_pair<String, DoubleReal>(String("none"), DoubleReal(0))));
      for (Size i = 0; i < SILAClabels.size(); ++i)       //SILACLabels MUST be in weight order!!!
      {
        std::vector<std::pair<String, DoubleReal> > one_label;
        for (UInt j = 0; j < SILAClabels[i].size(); ++j)
        {
          one_label.push_back(*(label_massshift.find(SILAClabels[i][j])));              // this dereferencing would break if all SILAClabels would not have been checked before!
        }
        labels.push_back(one_label);
      }
      msq.registerExperiment(exp, labels);       //add assays
      msq.assignUIDs();
    }
    MSQuantifications::QUANT_TYPES quant_type = MSQuantifications::MS1LABEL;
    msq.setAnalysisSummaryQuantType(quant_type);    //add analysis_summary_
	*/
	
	// ---------------------------
	// testing new data structures
	// ---------------------------
    
    // testing size types    
    /*double nonNaN = 100;
    double nan = std::numeric_limits<double>::quiet_NaN();
    std::cout << "double: " << (boost::math::isnan)(nonNaN) << " " << (boost::math::isnan)(nan) << "\n";*/
    
    // testing vector constructors
    /*std::vector<int> int_vector(100,-1);
    std::cout << "int vector: " << int_vector[0] << " " << int_vector[1] << " " << int_vector[99] << "\n";*/

	// ---------------------------
	// testing peak picking
	// ---------------------------
    
	//MzMLFile file_picked;
	//file_picked.store("picked.mzML", exp_picked);
	
	// ---------------------------
	// testing filtering
	// ---------------------------	
	
	// ---------------------------
	// testing clustering
	// ---------------------------	

    //--------------------------------------------------
    // estimate peak width
    //--------------------------------------------------

    /*LOG_DEBUG << "Estimating peak width..." << endl;
    PeakWidthEstimator::Result peak_width;
    try
    {
      peak_width = analyzer.estimatePeakWidth(exp);
    }
    catch (Exception::InvalidSize &)
    {
      writeLog_("Error: Unable to estimate peak width of input data.");
      return INCOMPATIBLE_INPUT_DATA;
    }


    if (in_filters == "")
    {
      //--------------------------------------------------
      // filter input data
      //--------------------------------------------------

      LOG_DEBUG << "Filtering input data..." << endl;
      analyzer.filterData(exp, peak_width, data); 

      //--------------------------------------------------
      // store filter results
      //--------------------------------------------------

      if (out_filters != "")
      {
        LOG_DEBUG << "Storing filtering results..." << endl;
        ConsensusMap map;
        for (std::vector<std::vector<SILACPattern> >::const_iterator it = data.begin(); it != data.end(); ++it)
        {
          analyzer.generateFilterConsensusByPattern(map, *it);
        }
        analyzer.writeConsensus(out_filters, map);
      }
    }
    else
    {
      //--------------------------------------------------
      // load filter results
      //--------------------------------------------------

      LOG_DEBUG << "Loading filtering results..." << endl;
      ConsensusMap map;
      analyzer.readConsensus(in_filters, map);
      analyzer.readFilterConsensusByPattern(map, data);
    }

    //--------------------------------------------------
    // clustering
    //--------------------------------------------------

    LOG_DEBUG << "Clustering data..." << endl;
    analyzer.clusterData(exp, peak_width, cluster_data, data);

    //--------------------------------------------------------------
    // write output
    //--------------------------------------------------------------

    if (out_debug_ != "")
    {
      LOG_DEBUG << "Writing debug output file..." << endl;
      std::ofstream out((out_debug_ + ".clusters.csv").c_str());

      vector<vector<DoubleReal> > massShifts = analyzer.getMassShifts(); // list of mass shifts

      // generate header
      out
      << std::fixed << std::setprecision(8)
      << "ID,RT,MZ_PEAK,CHARGE";
      for (UInt i = 1; i <= massShifts[0].size(); ++i)
      {
        out << ",DELTA_MASS_" << i + 1;
      }
      for (UInt i = 0; i <= massShifts[0].size(); ++i)
      {
        for (UInt j = 1; j <= isotopes_per_peptide_max; ++j)
        {
          out << ",INT_PEAK_" << i + 1 << '_' << j;
        }
      }
      out << ",MZ_RAW";
      for (UInt i = 0; i <= massShifts[0].size(); ++i)
      {
        for (UInt j = 1; j <= isotopes_per_peptide_max; ++j)
        {
          out << ",INT_RAW_" << i + 1 << '_' << j;
        }
      }
      for (UInt i = 0; i <= massShifts[0].size(); ++i)
      {
        for (UInt j = 1; j <= isotopes_per_peptide_max; ++j)
        {
          out << ",MZ_RAW_" << i + 1 << '_' << j;
        }
      }
      out << '\n';

      // write data
      UInt cluster_id = 0;
      for (vector<Clustering *>::const_iterator it = cluster_data.begin(); it != cluster_data.end(); ++it)
      {
        analyzer.generateClusterDebug(out, **it, cluster_id);
      }
    }

    if (out != "")
    {
      LOG_DEBUG << "Generating output consensus map..." << endl;
      ConsensusMap map;

      for (vector<Clustering *>::const_iterator it = cluster_data.begin(); it != cluster_data.end(); ++it)
      {
        analyzer.generateClusterConsensusByCluster(map, **it);
      }

      LOG_DEBUG << "Adding meta data..." << endl;
      // XXX: Need a map per mass shift
      ConsensusMap::FileDescriptions& desc = map.getFileDescriptions();
      Size id = 0;
      for (ConsensusMap::FileDescriptions::iterator it = desc.begin(); it != desc.end(); ++it)
      {
        if (test_mode_) it->second.filename = in; // skip path, since its not cross platform and complicates verification
        else it->second.filename = File::basename(in);
        // Write correct label
        // (this would crash if used without a label!)
        if (id > 0) it->second.label = ListUtils::concatenate(analyzer.getSILAClabels()[id - 1], ""); // skip first round (empty label is not listed)
        ++id;
      }

      std::set<DataProcessing::ProcessingAction> actions;
      actions.insert(DataProcessing::DATA_PROCESSING);
      actions.insert(DataProcessing::PEAK_PICKING);
      actions.insert(DataProcessing::FILTERING);
      actions.insert(DataProcessing::QUANTITATION);

      addDataProcessing_(map, getProcessingInfo_(actions));

      analyzer.writeConsensus(out, map);
      if (out_mzq_ != "")
      {
        LOG_DEBUG << "Generating output mzQuantML file..." << endl;
        ConsensusMap numap(map);
        //calc. ratios
        for (ConsensusMap::iterator cit = numap.begin(); cit != numap.end(); ++cit)
        {
          //~ make ratio templates
          std::vector<ConsensusFeature::Ratio> rts;
          for (std::vector<MSQuantifications::Assay>::const_iterator ait = msq.getAssays().begin() + 1; ait != msq.getAssays().end(); ++ait)
          {
            ConsensusFeature::Ratio r;
            r.numerator_ref_ = String(msq.getAssays().begin()->uid_);
            r.denominator_ref_ = String(ait->uid_);
            r.description_.push_back("Simple ratio calc");
            r.description_.push_back("light to medium/.../heavy");
            //~ "<cvParam cvRef=\"PSI-MS\" accession=\"MS:1001132\" name=\"peptide ratio\"/>"
            rts.push_back(r);
          }

          const ConsensusFeature::HandleSetType& feature_handles = cit->getFeatures();
          if (feature_handles.size() > 1)
          {
            std::set<FeatureHandle, FeatureHandle::IndexLess>::const_iterator fit = feature_handles.begin();             // this is unlabeled
            fit++;
            for (; fit != feature_handles.end(); ++fit)
            {
              Size ri = std::distance(feature_handles.begin(), fit);
              rts[ri - 1].ratio_value_ =  feature_handles.begin()->getIntensity() / fit->getIntensity();             // a proper silacalanyzer algo should never have 0-intensities so no 0devison ...
            }
          }

          cit->setRatios(rts);
        }
        msq.addConsensusMap(numap);        //add FeatureFinderMultiplex result

        //~ msq.addFeatureMap();//add FeatureFinderMultiplex evidencetrail as soon as clear what is realy contained in the featuremap
        //~ add AuditCollection - no such concept in TOPPTools yet
        analyzer.writeMzQuantML(out_mzq_, msq);
      }
    }

    if (out_clusters != "")
    {
      LOG_DEBUG << "Generating cluster output file..." << endl;
      ConsensusMap map;
      for (vector<Clustering *>::const_iterator it = cluster_data.begin(); it != cluster_data.end(); ++it)
      {
        UInt cluster_id = 0;
        analyzer.generateClusterConsensusByPattern(map, **it, cluster_id);
      }

      ConsensusMap::FileDescription & desc = map.getFileDescriptions()[0];
      desc.filename = in;
      desc.label = "Cluster";

      analyzer.writeConsensus(out_clusters, map);
    }

    if (out_features_ != "")
    {
      LOG_DEBUG << "Generating output feature map..." << endl;
      FeatureMap<> map;
      for (vector<Clustering *>::const_iterator it = cluster_data.begin(); it != cluster_data.end(); ++it)
      {
        analyzer.generateClusterFeatureByCluster(map, **it);
      }

      analyzer.writeFeatures(out_features_, map);
    }*/

    return EXECUTION_OK;
  }

};

int main(int argc, const char ** argv)
{
  TOPPFeatureFinderMultiplex tool;
  return tool.main(argc, argv);
}

//@endcond

