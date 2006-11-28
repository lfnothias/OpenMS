// -*- Mode: C++; tab-width: 2; -*-
// vi: set ts=2:
//
// --------------------------------------------------------------------------
//                   OpenMS Mass Spectrometry Framework
// --------------------------------------------------------------------------
//  Copyright (C) 2003-2006 -- Oliver Kohlbacher, Knut Reinert
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
// --------------------------------------------------------------------------
// $Maintainer: Marc Sturm $
// --------------------------------------------------------------------------

#ifndef OPENMS_FORMAT_MZDATAFILE_H
#define OPENMS_FORMAT_MZDATAFILE_H

#include <OpenMS/FORMAT/SchemaFile.h>
#include <OpenMS/FORMAT/HANDLERS/MzDataHandler.h>

namespace OpenMS
{
	/**
		@brief File adapter for MzData files

		@todo test all optional attributes (Thomas K.)
		
		@ingroup FileIO
	*/
	class MzDataFile : public Internal::SchemaFile
	{
		public:
			///Default constructor
			MzDataFile();
			///Destructor
			~MzDataFile();

			/**
				@brief Loads a map from a MzData file.

				@p map has to be a MSExperiment or have the same interface.
			*/
			template <typename MapType>
			void load(const String& filename, MapType& map) throw (Exception::FileNotFound, Exception::ParseError)
			{
				map.reset();
				
				Internal::MzDataHandler<MapType> handler(map,filename);
				parse_(filename, &handler);
			}

			/**
				@brief Stores a map in a MzData file.

				@p map has to be a MSExperiment or have the same interface.
			*/
			template <typename MapType>
			void store(const String& filename, const MapType& map)
			const throw (Exception::UnableToCreateFile)
			{
				Internal::MzDataHandler<MapType> handler(map,filename);
				save_(filename, &handler);
			}
	};

} // namespace OpenMS

#endif // OPENMS_FOMAT_MZXMLFILE_H
