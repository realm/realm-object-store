////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef REALM_ERROR_TYPE_HPP
#define REALM_ERROR_TYPE_HPP

namespace realm {

    enum class RealmErrorType
    {
        /** Thrown for any I/O related exception scenarios when a realm is opened. */
        AccessError,
        /** Thrown if the user does not have permission to open or create
         the specified file in the specified access mode when the realm is opened. */
        PermissionDenied,
        /** Thrown if no_create was specified and the file did already exist when the realm is opened. */
        Exists,
        /** Thrown if no_create was specified and the file was not found when the realm is opened. */
        NotFound,
        /** Thrown if the database file is currently open in another
         process which cannot share with the current process due to an
         architecture mismatch. */
        IncompatibleLockFile,

        MismatchedConfig,

        InvalidTransaction,

        IncorrectThread,

        UnitializedRealm
    };

}   // namespace realm

#endif /* defined(REALM_ERROR_TYPE_HPP) */
