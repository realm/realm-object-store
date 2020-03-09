//
//  core_remote_mongo_collection.hpp
//  realm-object-store
//
//  REPLACE HEADER
//

#ifndef REALM_CORE_REMOTE_MONGO_COLLECTION_HPP
#define REALM_CORE_REMOTE_MONGO_COLLECTION_HPP

#include <string>
#include <vector>
#include <realm/util/optional.hpp>
#include <json.hpp>
#include "core_remote_mongo_read_operation.hpp"

namespace realm {
namespace mongodb {

struct RemoteFindOptions {
    /// The maximum number of documents to return.
    long int limit;

    /// Limits the fields to return for all matching documents.
    std::string projection;

    /// The order in which to return matching documents.
    std::string sort;
};

struct RemoteInsertOneResult {
    // TODO: this needs to be of AnyBSONValue
    std::string inserted_id;
    const std::string inserted_id_key = "insertedId";
};

struct RemoteInsertManyResult {
    /// Map of the index of the inserted document to the id of the inserted document.
    std::map<long int, std::string> inserted_ids;
    const std::string inserted_ids_key = "insertedIds";
};

struct RemoteDeleteResult {
    int deleted_count;
    const std::string deleted_count_key = "deletedCount";
};

struct RemoteUpdateResult {
    
    int matched_count;
    int modified_count;
    std::string upserted_id;
    
    const std::string matched_count_key = "matchedCount";
    const std::string modified_count_key = "modifiedCount";
    const std::string upserted_id_key = "upsertedId";
};

struct RemoteFindOneAndModifyOptions {
    util::Optional<nlohmann::json> projection;
    util::Optional<nlohmann::json> sort;
    util::Optional<bool> upsert;
    util::Optional<bool> return_new_document;
};

template<typename T>
class CoreRemoteMongoCollection {
    
public:
    
    using CollectionType = T;
    using Document = nlohmann::json;

    /**
    * The name of this collection.
    */
    const std::string name;
    
    /**
    * The name of the database containing this collection.
    */
    const std::string database_name;

    CoreRemoteMongoCollection(std::string name, std::string database_name) : name(name), database_name(database_name) { }
    
    /**
    * Finds the documents in this collection which match the provided filter.
    *
    * - parameters:
    *   - filter: A `Document` that should match the query.
    *   - options: Optional `RemoteFindOptions` to use when executing the command.
    *
    * - important: Invoking this method by itself does not perform any network requests. You must call one of the
    *              methods on the resulting `CoreRemoteMongoReadOperation` instance to trigger the operation against
    *              the database.
    *
    * - returns: A `CoreRemoteMongoReadOperation` that allows retrieval of the resulting documents.
    */
    CoreRemoteMongoReadOperation<CollectionType> find(Document filter,
                                                      util::Optional<RemoteFindOptions> options);

    /**
    * Returns one document from a collection or view which matches the
    * provided filter. If multiple documents satisfy the query, this method
    * returns the first document according to the query's sort order or natural
    * order.
    *
    * - parameters:
    *   - filter: A `Document` that should match the query.
    *   - options: Optional `RemoteFindOptions` to use when executing the command.
    *
    * - returns: The resulting `Document` or nil if no such document exists
    */
    util::Optional<CollectionType> find_one(Document filter,
                                            util::Optional<RemoteFindOptions> options);
    
    /**
        * Runs an aggregation framework pipeline against this collection.
        *
        * - Parameters:
        *   - pipeline: An `[Document]` containing the pipeline of aggregation operations to perform.
        *
        * - important: Invoking this method by itself does not perform any network requests. You must call one of the
        *              methods on the resulting `CoreRemoteMongoReadOperation` instance to trigger the operation against
        *              the database.
        *
        * - returns: A `CoreRemoteMongoReadOperation` that allows retrieval of the resulting documents.
        */
    CoreRemoteMongoReadOperation<CollectionType> aggregate(std::vector<Document> pipline);

    /**
    * Counts the number of documents in this collection matching the provided filter.
    *
    * - Parameters:
    *   - filter: a `Document`, the filter that documents must match in order to be counted.
    *   - options: Optional `RemoteCountOptions` to use when executing the command.
    *
    * - Returns: The count of the documents that matched the filter.
    */
    int count(std::string filter, util::Optional<RemoteFindOptions> options);
    
    /**
    * Encodes the provided value to BSON and inserts it. If the value is missing an identifier, one will be
    * generated for it.
    *
    * - Parameters:
    *   - value: A `CollectionType` value to encode and insert.
    *
    * - Returns: The result of attempting to perform the insert.
    */
    RemoteInsertOneResult insert_one(CollectionType value);
    
    /**
    * Encodes the provided values to BSON and inserts them. If any values are missing identifiers,
    * they will be generated.
    *
    * - Parameters:
    *   - documents: The `CollectionType` values to insert.
    *
    * - Returns: The result of attempting to perform the insert.
    */
    RemoteInsertManyResult insert_many(std::vector<CollectionType> documents);
    
    /**
    * Deletes a single matching document from the collection.
    *
    * - Parameters:
    *   - filter: A `Document` representing the match criteria.
    *
    * - Returns: The result of performing the deletion.
    */
    RemoteDeleteResult delete_one(Document filter);

    /**
    * Deletes multiple documents
    *
    * - Parameters:
    *   - filter: Document representing the match criteria
    *
    * - Returns: The result of performing the deletion.
    */
    RemoteDeleteResult delete_many(Document filter);
    
    /**
    * Updates a single document matching the provided filter in this collection.
    *
    * - Parameters:
    *   - filter: A `Document` representing the match criteria.
    *   - update: A `Document` representing the update to be applied to a matching document.
    *   - options: Optional `RemoteUpdateOptions` to use when executing the command.
    *
    * - Returns: The result of attempting to update a document.
    */
    RemoteUpdateResult update_one(Document filter,
                                  Document update,
                                  util::Optional<RemoteFindOptions> options);

    /**
    * Updates multiple documents matching the provided filter in this collection.
    *
    * - Parameters:
    *   - filter: A `Document` representing the match criteria.
    *   - update: A `Document` representing the update to be applied to matching documents.
    *   - options: Optional `RemoteUpdateOptions` to use when executing the command.
    *
    * - Returns: The result of attempting to update multiple documents.
    */
    RemoteUpdateResult update_many(Document filter,
                                   Document update,
                                   util::Optional<RemoteFindOptions> options);

    /**
    * Updates a single document in a collection based on a query filter and
    * returns the document in either its pre-update or post-update form. Unlike
    * `updateOne`, this action allows you to atomically find, update, and
    * return a document with the same command. This avoids the risk of other
    * update operations changing the document between separate find and update
    * operations.
    *
    * - parameters:
    *   - filter: A `Document` that should match the query.
    *   - update: A `Document` describing the update.
    *   - options: Optional `RemoteFindOneAndModifyOptions` to use when executing the command.
    *
    * - returns: The resulting `Document` or nil if no such document exists
    */
    util::Optional<CollectionType> find_one_and_update(Document filter,
                                                       Document update,
                                                       util::Optional<RemoteFindOneAndModifyOptions> options);
    
    /**
    * Overwrites a single document in a collection based on a query filter and
    * returns the document in either its pre-replacement or post-replacement
    * form. Unlike `updateOne`, this action allows you to atomically find,
    * replace, and return a document with the same command. This avoids the
    * risk of other update operations changing the document between separate
    * find and update operations.
    *
    * - parameters:
    *   - filter: A `Document` that should match the query.
    *   - replacement: A `Document` describing the update.
    *   - options: Optional `RemoteFindOneAndModifyOptions` to use when executing the command.
    *
    * - returns: The resulting `Document` or nil if no such document exists
    */
    util::Optional<CollectionType> find_one_and_replace(Document filter,
                                                        Document replacement,
                                                        util::Optional<RemoteFindOneAndModifyOptions> options);

    /**
    * Removes a single document from a collection based on a query filter and
    * returns a document with the same form as the document immediately before
    * it was deleted. Unlike `deleteOne`, this action allows you to atomically
    * find and delete a document with the same command. This avoids the risk of
    * other update operations changing the document between separate find and
    * delete operations.
    *
    * - parameters:
    *   - filter: A `Document` that should match the query.
    *   - options: Optional `RemoteFindOneAndModifyOptions` to use when executing the command.
    *
    * - returns: The resulting `Document` or nil if no such document exists
    */
    util::Optional<CollectionType> find_one_and_delete(Document filter,
                                                       util::Optional<RemoteFindOneAndModifyOptions> options);

private:
    
    /**
    * Returns a document of database name and collection name
    */
    Document m_base_operation_args {
        { "database" , database_name },
        { "collection" , name }
    };
    
    /// Returns a version of the provided document with an ObjectId
    Document generate_object_id_if_missing(Document document);
    
    RemoteDeleteResult execute_delete(Document filter, bool multi);

    util::Optional<CollectionType> execute_find_one_and_modify(std::string func_name,
                                                               Document filter,
                                                               Document update,
                                                               util::Optional<RemoteFindOneAndModifyOptions> options);
    
    RemoteUpdateResult execute_update(Document filter,
                                      Document update,
                                      util::Optional<RemoteFindOneAndModifyOptions> options,
                                      bool multi);

};

} // namespace mongodb
} // namespace realm

#endif /* core_remote_mongo_collection_h */

