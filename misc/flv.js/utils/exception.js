/*
 * Copyright (C) 2016 Bilibili. All Rights Reserved.
 *
 * @author zheng qian <xqq@xqq.im>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

function RuntimeException(message) {
    this._message = message;
}

RuntimeException.prototype.name = function () {
    return 'RuntimeException';
}

RuntimeException.prototype.message = function () {
    return this._message;
}

RuntimeException.prototype.toString = function () {
    return this.name + ': ' + this.message;
}


function IllegalStateException(message) {
    // super(message);
    RuntimeException.call(this, message);
}

IllegalStateException.prototype.name = function () {
    return 'IllegalStateException';
}

function InvalidArgumentException(message) {
    RuntimeException.call(this, message);
}
InvalidArgumentException.prototype.name = function () {
    return 'InvalidArgumentException';
}

function NotImplementedException(message) {
    RuntimeException.call(this, message);
    // super(message);
}

NotImplementedException.prototype.name = function () {
    return 'NotImplementedException';
}
