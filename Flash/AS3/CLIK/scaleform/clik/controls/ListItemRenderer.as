﻿/**
 *  The ListItemRenderer derives from the CLIK Button class and extends it to include list-related properties that are useful for its container components.
    However, it is not designed to be a standalone component, instead it is only used in conjunction with the ScrollingList, TileList and DropdownMenu components.

    <br>
    <b>Inspectable Properties</b>
    <br>
    Since the ListItemRenderer’s are controlled by a container component and never configured manually by a user, they contain only a small subset of the inspectable properties of the Button.
    <ul>
        <li><i>label</i>: Sets the label of the ListItemRenderer.</li>
        <li><i>visible</i>: Hides the button if set to false.</li>
        <li><i>enabled</i>: Disables the button if set to true.</li>
    </ul>
    
    <br>
    <b>States</b>
    <br>
    Since it can be selected inside a container component, the ListItemRenderer requires the selected set of keyframes to denote its selected state. This component’s states include
    <ul>
        <li>an up or default state.</li>
        <li>an over state when the mouse cursor is over the component, or when it is focused.</li>
        <li>a down state when the button is pressed.</li>
        <li>a disabled state.</li>
        <li>a selected_up or default state.</li>
        <li>a selected_over state when the mouse cursor is over the component, or when it is focused.</li>
        <li>a selected_down state when the button is pressed.</li>
        <li>a selected_disabled state.</li>
    </ul>
    
    These are the minimal set of keyframes that should be in a ListItemRenderer. The extended set of states and keyframes supported by the Button component, and consequently the ListItemRenderer component, are described in the Getting Started with CLIK Buttons document.
    
    <br>
    <b>Events</b>
    <br>
    All event callbacks receive a single Event parameter that contains relevant information about the event. The following properties are common to all events.
    <ul>
        <li><i>type</i>: The event type.</li>
        <li><i>target</i>: The target that generated the event.</li>
    </ul>
        
    The events generated by the ListItemRenderer component are the same as the Button component. The properties listed next to the event are provided in addition to the common properties.
    <br>
    <br>
    Generally, rather than listening for the events dispatched by ListItemRenderer’s, users should add an event listener to the List itself. 
    Lists will dispatch ListEvents including ITEM_PRESS and ITEM_CLICK when interaction with one of its child ListItemRenderers occurs. 
    This allows users to add one EventListener to a List for a mouse click event (ListEvent.ITEM_CLICK), rather than one EventListener on each ListItemRenderer within said List.
    <ul>
        <li><i>ComponentEvent.SHOW</i>: The visible property has been set to true at runtime.</li>
        <li><i>ComponentEvent.HIDE</i>: The visible property has been set to false at runtime.</li>
        <li><i>ComponentEvent.STATE_CHANGE</i>: The component's state has changed.</li>
        <li><i>FocusHandlerEvent.FOCUS_IN</i>: The component has received focus.</li>
        <li><i>FocusHandlerEvent.FOCUS_OUT</i>: The component has lost focus.</li>
        <li><i>Event.SELECT</i>: The selected property has changed.</li>
        <li><i>ButtonEvent.PRESS</i>: The button has been pressed.</li>
        <li><i>ButtonEvent.CLICK</i>: The button has been clicked.</li>
        <li><i>ButtonEvent.DRAG_OVER</i>: The mouse cursor has been dragged over the button (while the left mouse button is pressed).</li>
        <li><i>ButtonEvent.DRAG_OUT</i>: The mouse cursor has been dragged out of the button (while the left mouse button is pressed).</li>
        <li><i>ButtonEvent.RELEASE_OUTSIDE</i>: The mouse cursor has been dragged out of the button and the left mouse button has been released.</li>
    </ul>
*/
    
/**************************************************************************

Filename    :   ListItemRenderer.as

Copyright   :   Copyright 2011 Autodesk, Inc. All Rights reserved.

Use of this software is subject to the terms of the Autodesk license
agreement provided at the time of installation or download, or which
otherwise accompanies this software in either electronic or hard copy form.

**************************************************************************/
    
package scaleform.clik.controls {
    
    import flash.events.FocusEvent;
    
    import scaleform.clik.core.UIComponent;
    import scaleform.clik.data.ListData;
    import scaleform.clik.interfaces.IListItemRenderer;
    
    public class ListItemRenderer extends Button implements IListItemRenderer {
        
    // Constants:
        
    // Public Properties:
        
    // Protected Properties:
        protected var _index:uint = 0; // Index of the ListItemRenderer
        protected var _selectable:Boolean = true;
        
    // UI Elements:
        
    // Initialization:
        public function ListItemRenderer() {
            super();
        }
        
    // Public Getter / Setters:
        // Override for focusable since ListItemRenderers should never acquire focus.
        /** @exclude */ 
        override public function get focusable():Boolean { return _focusable; }
        override public function set focusable(value:Boolean):void { } 
        
        /** The index of this ListItemRenderer relative to its owner (generally, a subclass of CoreList). */
        public function get index():uint { return _index; }
        public function set index(value:uint):void { _index = value; }
        
        /** UNIMPLEMENTED! true if this ListItemRenderer can be selected in the List; false otherwise. */
        public function get selectable():Boolean { return _selectable; }
        public function set selectable(value:Boolean):void { _selectable = value; }
        
    // Public Methods:
        /**
         * Set the list data relevant to the ListItemRenderer. Each time the item changes, or is redrawn by the {@code owner}, the ListItemRenderer is updated using this method.
         * @param listData An object which contains information for this ListItemRenderer relative to the List (index, selected state, label).
         */
        public function setListData(listData:ListData):void {
            index = listData.index;
            selected = listData.selected; // Maybe this should be .selected and then skip the setState below (since we set displayFocus).
            label = listData.label || "";
            // setState( "up" ); // Refresh the component, including the timeline state.
        }
        
        /**
         * Sets data from the {@code dataProvider} to the renderer.
         * @param data The data associated with this ListItemRenderer.
         */
        public function setData(data:Object):void {
            this.data = data;
        }
        
        /** @exclude */
        override public function toString():String {
            return "[CLIK ListItemRenderer " + index + ", " + name + "]";
        }
        
    // Protected Methods:
        override protected function configUI():void {
            super.configUI();
            focusTarget = owner; // The component sets the focusTarget to its owner instead of vice-versa.  This allows sub-classes to override this behaviour.
            _focusable = tabEnabled = tabChildren = mouseChildren = false;
        }
    }
}