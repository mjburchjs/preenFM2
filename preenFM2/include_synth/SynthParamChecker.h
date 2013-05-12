/*
 * Copyright 2013 Xavier Hosxe
 *
 * Author: Xavier Hosxe (xavier . hosxe (at) gmail . com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef SYNTHPARAMCHECKER_H_
#define SYNTHPARAMCHECKER_H_


class SynthParamChecker {
public:
    virtual void checkNewParamValue(int timbre, int currentRow, int encoder, float oldValue, float *newValue) = 0;

    SynthParamChecker* nextChecker;
};

#endif /* SYNTHPARAMCHECKER_H_ */
