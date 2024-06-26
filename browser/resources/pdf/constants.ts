// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="enable_pdf_ink2">
// Some brushes don't need colors and a size, but the brushes that do should
// have values for the red, green, and blue colors and a value for the size.
export interface AnnotationBrush {
  type: AnnotationBrushType;
  params?: AnnotationBrushParams;
}

// The annotation brush params, required for certain brush types.
export interface AnnotationBrushParams {
  colorR: number;
  colorG: number;
  colorB: number;
  size: number;
}

// The different types of annotation brushes.
export enum AnnotationBrushType {
  ERASER = 'eraser',
  HIGHLIGHTER = 'highlighter',
  PEN = 'pen',
}
// </if>

export interface Attachment {
  name: string;
  size: number;
  readable: boolean;
}

export enum DisplayAnnotationsAction {
  DISPLAY_ANNOTATIONS = 'display-annotations',
  HIDE_ANNOTATIONS = 'hide-annotations',
}

export interface DocumentMetadata {
  author: string;
  canSerializeDocument: boolean;
  creationDate: string;
  creator: string;
  fileSize: string;
  keywords: string;
  linearized: boolean;
  modDate: string;
  pageSize: string;
  producer: string;
  subject: string;
  title: string;
  version: string;
}

/** Enumeration of page fitting types and bounding box fitting types. */
export enum FittingType {
  NONE = 'none',
  FIT_TO_PAGE = 'fit-to-page',
  FIT_TO_WIDTH = 'fit-to-width',
  FIT_TO_HEIGHT = 'fit-to-height',
  FIT_TO_BOUNDING_BOX = 'fit-to-bounding-box',
  FIT_TO_BOUNDING_BOX_WIDTH = 'fit-to-bounding-box-width',
  FIT_TO_BOUNDING_BOX_HEIGHT = 'fit-to-bounding-box-height',
}

export interface NamedDestinationMessageData {
  messageId: string;
  pageNumber: number;
  namedDestinationView?: string;
}

/**
 * Enumeration of save message request types. Must match `SaveRequestType` in
 * pdf/pdf_view_web_plugin.h.
 */
export enum SaveRequestType {
  ANNOTATION,
  ORIGINAL,
  EDITED,
}

export interface Point {
  x: number;
  y: number;
}

export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

export type ExtendedKeyEvent = KeyboardEvent&{
  fromScriptingAPI?: boolean,
  fromPlugin?: boolean,
};
