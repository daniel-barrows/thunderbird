/*
Copyright 2015, 2016 OpenMarket Ltd

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
"use strict";
/**
 * @module models/room-summary
 */

/**
 * Construct a new Room Summary. A summary can be used for display on a recent
 * list, without having to load the entire room list into memory.
 * @constructor
 * @param {string} roomId Required. The ID of this room.
 * @param {Object} info Optional. The summary info. Additional keys are supported.
 * @param {string} info.title The title of the room (e.g. <code>m.room.name</code>)
 * @param {string} info.desc The description of the room (e.g.
 * <code>m.room.topic</code>)
 * @param {Number} info.numMembers The number of joined users.
 * @param {string[]} info.aliases The list of aliases for this room.
 * @param {Number} info.timestamp The timestamp for this room.
 */
function RoomSummary(roomId, info) {
    this.roomId = roomId;
    this.info = info;
}

/**
 * The RoomSummary class.
 */
module.exports = RoomSummary;
