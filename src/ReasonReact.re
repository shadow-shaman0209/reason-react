type reactClass;

type jsProps;

type reactElement = React.element;

type reactRef;

[@bs.val] external null: reactElement = "null";

external string: string => reactElement = "%identity";

external array: array(reactElement) => reactElement = "%identity";

external refToJsObj: reactRef => Js.t({..}) = "%identity";

[@bs.splice] [@bs.val] [@bs.module "react"]
external createElement:
  (reactClass, ~props: Js.t({..})=?, array(reactElement)) => reactElement =
  "createElement";

[@bs.splice] [@bs.module "react"]
external cloneElement:
  (reactElement, ~props: Js.t({..})=?, array(reactElement)) => reactElement =
  "cloneElement";

[@bs.val] [@bs.module "react"]
external createElementVerbatim: 'a = "createElement";

let createDomElement = (s, ~props, children) => {
  let vararg =
    [|Obj.magic(s), Obj.magic(props)|] |> Js.Array.concat(children);
  /* Use varargs to avoid warnings on duplicate keys in children */
  Obj.magic(createElementVerbatim)##apply(Js.Nullable.null, vararg);
};

[@bs.val] external magicNull: 'a = "null";

type reactClassInternal = reactClass;

type renderNotImplemented =
  | RenderNotImplemented;

type stateless = unit;

type noRetainedProps = unit;

type actionless = unit;

/***
 * Elements are what JSX blocks become. They represent the *potential* for a
 * component instance and state to be created / updated. They are not yet
 * instances.
 */
type element =
  | Element(component('state, 'retainedProps, 'action)): element
and jsPropsToReason('jsProps, 'state, 'retainedProps, 'action) =
  'jsProps => component('state, 'retainedProps, 'action)
and uncurriedJsPropsToReason('jsProps, 'state, 'retainedProps, 'action) =
  (. 'jsProps) => component('state, 'retainedProps, 'action)
/***
 * Type of hidden field for Reason components that use JS components
 */
and jsElementWrapped =
  option(
    (
      ~key: Js.nullable(string),
      ~ref: Js.nullable(Js.nullable(reactRef) => unit)
    ) =>
    reactElement,
  )
and update('state, 'retainedProps, 'action) =
  | NoUpdate
  | Update('state)
  | SideEffects(self('state, 'retainedProps, 'action) => unit)
  | UpdateWithSideEffects(
      'state,
      self('state, 'retainedProps, 'action) => unit,
    )
/***
 * Granularly types state, and initial state as being independent, so that we
 * may include a template that all instances extend from.
 */
and componentSpec(
  'state,
  'initialState,
  'retainedProps,
  'initialRetainedProps,
  'action,
) = {
  debugName: string,
  reactClassInternal,
  /* Keep here as a way to prove that the API may be implemented soundly */
  mutable handedOffState: ref(option('state)),
  willReceiveProps: self('state, 'retainedProps, 'action) => 'state,
  didMount: self('state, 'retainedProps, 'action) => unit,
  didUpdate: oldNewSelf('state, 'retainedProps, 'action) => unit,
  willUnmount: self('state, 'retainedProps, 'action) => unit,
  willUpdate: oldNewSelf('state, 'retainedProps, 'action) => unit,
  shouldUpdate: oldNewSelf('state, 'retainedProps, 'action) => bool,
  render: self('state, 'retainedProps, 'action) => reactElement,
  initialState: unit => 'initialState,
  retainedProps: 'initialRetainedProps,
  reducer: ('action, 'state) => update('state, 'retainedProps, 'action),
  jsElementWrapped,
}
and component('state, 'retainedProps, 'action) =
  componentSpec('state, 'state, 'retainedProps, 'retainedProps, 'action)
and self('state, 'retainedProps, 'action) = {
  handle:
    'payload.
    (('payload, self('state, 'retainedProps, 'action)) => unit, 'payload) =>
    unit,

  state: 'state,
  retainedProps: 'retainedProps,
  send: 'action => unit,
  onUnmount: (unit => unit) => unit,
}
and oldNewSelf('state, 'retainedProps, 'action) = {
  oldSelf: self('state, 'retainedProps, 'action),
  newSelf: self('state, 'retainedProps, 'action),
};

type jsComponentThis('state, 'props, 'retainedProps, 'action) = {
  .
  "state": totalState('state, 'retainedProps, 'action),
  "props": {. "reasonProps": 'props},
  "setState":
    [@bs.meth] (
      (
        (totalState('state, 'retainedProps, 'action), 'props) =>
        totalState('state, 'retainedProps, 'action),
        Js.nullable(unit => unit)
      ) =>
      unit
    ),
  "jsPropsToReason":
    option(
      uncurriedJsPropsToReason('props, 'state, 'retainedProps, 'action),
    ),
}
/***
 * `totalState` tracks all of the internal reason API bookkeeping.
 *
 * Since we will mutate `totalState` in `shouldComponentUpdate`, and since
 * there's no guarantee that returning true from `shouldComponentUpdate`
 * guarantees that a component's update *actually* takes place (it could get
 * rolled back by Fiber etc), then we should put all properties that we
 * mutate directly on the totalState, so that when Fiber makes backup shallow
 * backup copies of `totalState`, our changes can be rolled back correctly
 * even when we mutate them.
 */
and totalState('state, 'retainedProps, 'action) = {. "reasonState": 'state};

let anyToUnit = _ => ();

let anyToTrue = _ => true;

let willReceivePropsDefault = ({state}) => state;

let renderDefault = _self => string("RenderNotImplemented");

let initialStateDefault = () => ();

let reducerDefault:
  ('action, 'state) => update('state, 'retainedProps, 'action) =
  (_action, _state) => NoUpdate;

let convertPropsIfTheyreFromJs = (props, jsPropsToReason, debugName) => {
  let props = Obj.magic(props);
  switch (Js.Nullable.toOption(props##reasonProps), jsPropsToReason) {
  | (Some(props), _) => props
  | (None, Some(toReasonProps)) => Element(toReasonProps(. props))
  | (None, None) =>
    raise(
      Invalid_argument(
        "A JS component called the Reason component "
        ++ debugName
        ++ " which didn't implement the JS->Reason React props conversion.",
      ),
    )
  };
};

let createClass =
    (type reasonState, type retainedProps, type action, debugName): reactClass =>
  ReasonReactOptimizedCreateClass.createClass(.
    [@bs]
    {
      /***
       * TODO: Null out fields that aren't overridden beyond defaults in
       * `component`. React optimizes components that don't implement
       * lifecycles!
       */
      val displayName = debugName;
      val mutable subscriptions = Js.null;
      /***
       * TODO: Avoid allocating this every time we need it. Should be doable.
       */
      pub self = (state, retainedProps) => {
        handle: Obj.magic(this##handleMethod),
        send: Obj.magic(this##sendMethod),
        state,
        retainedProps,
        onUnmount: Obj.magic(this##onUnmountMethod),
      };
      pub getInitialState = (): totalState('state, 'retainedProps, 'action) => {
        let thisJs:
          jsComponentThis(reasonState, element, retainedProps, action) = [%bs.raw
          "this"
        ];
        let convertedReasonProps =
          convertPropsIfTheyreFromJs(
            thisJs##props,
            thisJs##jsPropsToReason,
            debugName,
          );
        let Element(component) = convertedReasonProps;
        let initialReasonState = component.initialState();
        {"reasonState": Obj.magic(initialReasonState)};
      };
      pub componentDidMount = () => {
        let thisJs:
          jsComponentThis(reasonState, element, retainedProps, action) = [%bs.raw
          "this"
        ];
        let convertedReasonProps =
          convertPropsIfTheyreFromJs(
            thisJs##props,
            thisJs##jsPropsToReason,
            debugName,
          );
        let Element(component) = convertedReasonProps;
        let curTotalState = thisJs##state;
        let curReasonState = curTotalState##reasonState;
        let self =
          this##self(curReasonState, Obj.magic(component.retainedProps));
        let self = Obj.magic(self);
        if (component.didMount !== anyToUnit) {
          component.didMount(self);
        };
      };
      pub componentDidUpdate = (prevProps, prevState) => {
        let thisJs:
          jsComponentThis(reasonState, element, retainedProps, action) = [%bs.raw
          "this"
        ];
        let curState = thisJs##state;
        let curReasonState = curState##reasonState;
        let newJsProps = thisJs##props;
        let newConvertedReasonProps =
          convertPropsIfTheyreFromJs(
            newJsProps,
            thisJs##jsPropsToReason,
            debugName,
          );
        let Element(newComponent) = newConvertedReasonProps;
        if (newComponent.didUpdate !== anyToUnit) {
          let oldConvertedReasonProps =
            prevProps === newJsProps ?
              newConvertedReasonProps :
              convertPropsIfTheyreFromJs(
                prevProps,
                thisJs##jsPropsToReason,
                debugName,
              );
          let Element(oldComponent) = oldConvertedReasonProps;
          let prevReasonState = prevState##reasonState;
          let prevReasonState = Obj.magic(prevReasonState);
          let newSelf =
            this##self(
              curReasonState,
              Obj.magic(newComponent.retainedProps),
            );
          let newSelf = Obj.magic(newSelf);
          /* bypass this##self call for small perf boost */
          let oldSelf =
            Obj.magic({
              ...newSelf,
              state: prevReasonState,
              retainedProps: oldComponent.retainedProps,
            });
          newComponent.didUpdate({oldSelf, newSelf});
        };
      };
      /* pub componentWillMount .. TODO (or not?) */
      pub componentWillUnmount = () => {
        let thisJs:
          jsComponentThis(reasonState, element, retainedProps, action) = [%bs.raw
          "this"
        ];
        let convertedReasonProps =
          convertPropsIfTheyreFromJs(
            thisJs##props,
            thisJs##jsPropsToReason,
            debugName,
          );
        let Element(component) = convertedReasonProps;
        let curState = thisJs##state;
        let curReasonState = curState##reasonState;
        if (component.willUnmount !== anyToUnit) {
          let self =
            this##self(curReasonState, Obj.magic(component.retainedProps));
          let self = Obj.magic(self);
          component.willUnmount(self);
        };
        switch (Js.Null.toOption(this##subscriptions)) {
        | None => ()
        | Some(subs) => Js.Array.forEach(unsubscribe => unsubscribe(), subs)
        };
      };
      /***
       * If we are even getting this far, we've already done all the logic for
       * detecting unnecessary updates in shouldComponentUpdate. We know at
       * this point that we need to rerender, and we've even *precomputed* the
       * render result (subelements)!
       */
      pub componentWillUpdate = (nextProps, nextState: totalState(_)) => {
        let thisJs:
          jsComponentThis(reasonState, element, retainedProps, action) = [%bs.raw
          "this"
        ];
        let newConvertedReasonProps =
          convertPropsIfTheyreFromJs(
            nextProps,
            thisJs##jsPropsToReason,
            debugName,
          );
        let Element(newComponent) = newConvertedReasonProps;
        if (newComponent.willUpdate !== anyToUnit) {
          let oldJsProps = thisJs##props;
          /* Avoid converting again the props that are just the same as curProps. */
          let oldConvertedReasonProps =
            nextProps === oldJsProps ?
              newConvertedReasonProps :
              convertPropsIfTheyreFromJs(
                oldJsProps,
                thisJs##jsPropsToReason,
                debugName,
              );
          let Element(oldComponent) = oldConvertedReasonProps;
          let curState = thisJs##state;
          let curReasonState = curState##reasonState;
          let curReasonState = Obj.magic(curReasonState);
          let nextReasonState = nextState##reasonState;
          let newSelf =
            this##self(
              nextReasonState,
              Obj.magic(newComponent.retainedProps),
            );
          let newSelf = Obj.magic(newSelf);
          /* bypass this##self call for small perf boost */
          let oldSelf =
            Obj.magic({
              ...newSelf,
              state: curReasonState,
              retainedProps: oldComponent.retainedProps,
            });
          newComponent.willUpdate({oldSelf, newSelf});
        };
      };
      /***
       * One interesting part of the new Reason React API. There isn't a need
       * for a separate `willReceiveProps` function. The primary `create` API
       * is *always* receiving props.
       */
      pub componentWillReceiveProps = nextProps => {
        let thisJs:
          jsComponentThis(reasonState, element, retainedProps, action) = [%bs.raw
          "this"
        ];
        let newConvertedReasonProps =
          convertPropsIfTheyreFromJs(
            nextProps,
            thisJs##jsPropsToReason,
            debugName,
          );
        let Element(newComponent) = Obj.magic(newConvertedReasonProps);
        if (newComponent.willReceiveProps !== willReceivePropsDefault) {
          let oldJsProps = thisJs##props;
          /* Avoid converting again the props that are just the same as curProps. */
          let oldConvertedReasonProps =
            nextProps === oldJsProps ?
              newConvertedReasonProps :
              convertPropsIfTheyreFromJs(
                oldJsProps,
                thisJs##jsPropsToReason,
                debugName,
              );
          let Element(oldComponent) = oldConvertedReasonProps;
          thisJs##setState(
            (curTotalState, _) => {
              let curReasonState = Obj.magic(curTotalState##reasonState);
              let oldSelf =
                Obj.magic(
                  this##self(
                    curReasonState,
                    Obj.magic(oldComponent.retainedProps),
                  ),
                );
              let nextReasonState =
                Obj.magic(newComponent.willReceiveProps(oldSelf));
              if (nextReasonState !== curTotalState) {
                let nextTotalState: totalState(_) = {
                  "reasonState": nextReasonState,
                };
                let nextTotalState = Obj.magic(nextTotalState);
                nextTotalState;
              } else {
                curTotalState;
              };
            },
            Js.Nullable.null,
          );
        };
      };
      /***
       * shouldComponentUpdate is invoked any time props change, or new state
       * updates occur.
       *
       * The easiest way to think about this method, is:
       * - "Should component have its componentWillUpdate method called,
       * followed by its render() method?",
       *
       * Therefore the component.shouldUpdate becomes a hook solely to perform
       * performance optimizations through.
       */
      pub shouldComponentUpdate = (nextJsProps, nextState, _) => {
        let thisJs:
          jsComponentThis(reasonState, element, retainedProps, action) = [%bs.raw
          "this"
        ];
        let curJsProps = thisJs##props;

        /***
         * Now, we inspect the next state that we are supposed to render, and ensure that
         * - We have enough information to answer "should update?"
         * - We have enough information to render() in the event that the answer is "true".
         *
         * If we can detect that props have changed update has occured,
         * we ask the component's shouldUpdate if it would like to update - defaulting to true.
         */
        let oldConvertedReasonProps =
          convertPropsIfTheyreFromJs(
            thisJs##props,
            thisJs##jsPropsToReason,
            debugName,
          );
        /* Avoid converting again the props that are just the same as curProps. */
        let newConvertedReasonProps =
          nextJsProps === curJsProps ?
            oldConvertedReasonProps :
            convertPropsIfTheyreFromJs(
              nextJsProps,
              thisJs##jsPropsToReason,
              debugName,
            );
        let Element(oldComponent) = oldConvertedReasonProps;
        let Element(newComponent) = newConvertedReasonProps;
        let nextReasonState = nextState##reasonState;
        let newSelf =
          this##self(nextReasonState, Obj.magic(newComponent.retainedProps));
        if (newComponent.shouldUpdate !== anyToTrue) {
          let curState = thisJs##state;
          let curReasonState = curState##reasonState;
          let curReasonState = Obj.magic(curReasonState);
          let newSelf = Obj.magic(newSelf);
          /* bypass this##self call for small perf boost */
          let oldSelf =
            Obj.magic({
              ...newSelf,
              state: curReasonState,
              retainedProps: oldComponent.retainedProps,
            });
          newComponent.shouldUpdate({oldSelf, newSelf});
        } else {
          true;
        };
      };
      pub onUnmountMethod = subscription =>
        switch (Js.Null.toOption(this##subscriptions)) {
        | None => this##subscriptions #= Js.Null.return([|subscription|])
        | Some(subs) => ignore(Js.Array.push(subscription, subs))
        };
      pub handleMethod = callback => {
        let thisJs:
          jsComponentThis(reasonState, element, retainedProps, action) = [%bs.raw
          "this"
        ];
        callbackPayload => {
          let curState = thisJs##state;
          let curReasonState = curState##reasonState;
          let convertedReasonProps =
            convertPropsIfTheyreFromJs(
              thisJs##props,
              thisJs##jsPropsToReason,
              debugName,
            );
          let Element(component) = convertedReasonProps;
          callback(
            callbackPayload,
            Obj.magic(
              this##self(curReasonState, Obj.magic(component.retainedProps)),
            ),
          );
        };
      };
      pub sendMethod = (action: 'action) => {
        let thisJs:
          jsComponentThis(reasonState, element, retainedProps, action) = [%bs.raw
          "this"
        ];
        let convertedReasonProps =
          convertPropsIfTheyreFromJs(
            thisJs##props,
            thisJs##jsPropsToReason,
            debugName,
          );
        let Element(component) = convertedReasonProps;
        if (component.reducer !== reducerDefault) {
          let sideEffects = ref(ignore);
          /* allow side-effects to be executed here */
          let partialStateApplication = component.reducer(Obj.magic(action));
          thisJs##setState(
            (curTotalState, _) => {
              let curReasonState = curTotalState##reasonState;
              let reasonStateUpdate =
                partialStateApplication(Obj.magic(curReasonState));
              if (reasonStateUpdate === NoUpdate) {
                magicNull;
              } else {
                let reasonStateUpdate = Obj.magic(reasonStateUpdate);
                let nextTotalState =
                  switch (reasonStateUpdate) {
                  | NoUpdate => curTotalState
                  | Update(nextReasonState) => {
                      "reasonState": nextReasonState,
                    }
                  | SideEffects(performSideEffects) =>
                    sideEffects.contents = performSideEffects;
                    curTotalState;
                  | UpdateWithSideEffects(nextReasonState, performSideEffects) =>
                    sideEffects.contents = performSideEffects;
                    {"reasonState": nextReasonState};
                  };
                if (nextTotalState !== curTotalState) {
                  nextTotalState;
                } else {
                  magicNull;
                };
              };
            },
            Js.Nullable.return(
              this##handleMethod(((), self) => sideEffects.contents(self)),
            ),
          );
        };
      };
      /***
       * In order to ensure we always operate on freshest props / state, and to
       * support the API that "reduces" the next state along with the next
       * rendering, with full acccess to named argument props in the closure,
       * we always *pre* compute the render result.
       */
      pub render = () => {
        let thisJs:
          jsComponentThis(reasonState, element, retainedProps, action) = [%bs.raw
          "this"
        ];
        let convertedReasonProps =
          convertPropsIfTheyreFromJs(
            thisJs##props,
            thisJs##jsPropsToReason,
            debugName,
          );
        let Element(created) = Obj.magic(convertedReasonProps);
        let component = created;
        let curState = thisJs##state;
        let curReasonState = Obj.magic(curState##reasonState);
        let self =
          Obj.magic(
            this##self(curReasonState, Obj.magic(component.retainedProps)),
          );
        component.render(self);
      }
    },
  );

let basicComponent = debugName => {
  let componentTemplate = {
    reactClassInternal: createClass(debugName),
    debugName,
    /* Keep here as a way to prove that the API may be implemented soundly */
    handedOffState: {
      contents: None,
    },
    didMount: anyToUnit,
    willReceiveProps: willReceivePropsDefault,
    didUpdate: anyToUnit,
    willUnmount: anyToUnit,
    willUpdate: anyToUnit,
    /***
     * Called when component will certainly mount at some point - and may be
     * called on the sever for server side React rendering.
     */
    shouldUpdate: anyToTrue,
    render: renderDefault,
    initialState: initialStateDefault,
    reducer: reducerDefault,
    jsElementWrapped: None,
    retainedProps: (),
  };
  componentTemplate;
};

let statelessComponent =
    debugName: component(stateless, noRetainedProps, actionless) =>
  basicComponent(debugName);

let statelessComponentWithRetainedProps =
    debugName
    : componentSpec(
        stateless,
        stateless,
        'retainedProps,
        noRetainedProps,
        actionless,
      ) =>
  basicComponent(debugName);

let reducerComponent =
    debugName
    : componentSpec(
        'state,
        stateless,
        noRetainedProps,
        noRetainedProps,
        'action,
      ) =>
  basicComponent(debugName);

let reducerComponentWithRetainedProps =
    debugName
    : componentSpec(
        'state,
        stateless,
        'retainedProps,
        noRetainedProps,
        'action,
      ) =>
  basicComponent(debugName);

/***
 * Convenience for creating React elements before we have a better JSX transform.  Hopefully this makes it
 * usable to build some components while waiting to migrate the JSX transform to the next API.
 *
 * Constrain the component here instead of relying on the Element constructor which would lead to confusing
 * error messages.
 */
let element =
    (
      ~key: string=Obj.magic(Js.Nullable.undefined),
      ~ref: Js.nullable(reactRef) => unit=Obj.magic(Js.Nullable.undefined),
      component: component('state, 'retainedProps, 'action),
    ) => {
  let element = Element(component);
  switch (component.jsElementWrapped) {
  | Some(jsElementWrapped) =>
    jsElementWrapped(
      ~key=Js.Nullable.return(key),
      ~ref=Js.Nullable.return(ref),
    )
  | None =>
    createElement(
      component.reactClassInternal,
      ~props={"key": key, "ref": ref, "reasonProps": element},
      [||],
    )
  };
};

let wrapReasonForJs =
    (
      ~component,
      jsPropsToReason:
        jsPropsToReason('jsProps, 'state, 'retainedProps, 'action),
    ) => {
  let jsPropsToReason:
    jsPropsToReason(jsProps, 'state, 'retainedProps, 'action) =
    Obj.magic(jsPropsToReason) /* cast 'jsProps to jsProps */;
  let uncurriedJsPropsToReason:
    uncurriedJsPropsToReason(jsProps, 'state, 'retainedProps, 'action) =
    (. jsProps) => jsPropsToReason(jsProps);
  Obj.magic(component.reactClassInternal)##prototype##jsPropsToReason
  #= Some(uncurriedJsPropsToReason);
  component.reactClassInternal;
};

module WrapProps = {
  /* We wrap the props for reason->reason components, as a marker that "these props were passed from another
     reason component" */
  let wrapProps =
      (
        ~reactClass,
        ~props,
        children,
        ~key: Js.nullable(string),
        ~ref: Js.nullable(Js.nullable(reactRef) => unit),
      ) => {
    let props =
      Js.Obj.assign(
        Js.Obj.assign(Js.Obj.empty(), Obj.magic(props)),
        {"ref": ref, "key": key},
      );
    let varargs =
      [|Obj.magic(reactClass), Obj.magic(props)|]
      |> Js.Array.concat(Obj.magic(children));
    /* Use varargs under the hood */
    Obj.magic(createElementVerbatim)##apply(Js.Nullable.null, varargs);
  };
  let dummyInteropComponent = basicComponent("interop");
  let wrapJsForReason =
      (~reactClass, ~props, children)
      : component(stateless, noRetainedProps, _) => {
    let jsElementWrapped = Some(wrapProps(~reactClass, ~props, children));
    {...dummyInteropComponent, jsElementWrapped};
  };
};

let wrapJsForReason = WrapProps.wrapJsForReason;

[@bs.module "react"] external fragment: 'a = "Fragment";

module Router = {
  [@bs.get] external location: Dom.window => Dom.location = "location";

  [@bs.send]
  /* actually the cb is Dom.event => unit, but let's restrict the access for now */
  external addEventListener: (Dom.window, string, unit => unit) => unit =
    "addEventListener";

  [@bs.send]
  external removeEventListener: (Dom.window, string, unit => unit) => unit =
    "";

  [@bs.send]
  external dispatchEvent: (Dom.window, Dom.event) => unit = "dispatchEvent";

  [@bs.get] external pathname: Dom.location => string = "pathname";

  [@bs.get] external hash: Dom.location => string = "hash";

  [@bs.get] external search: Dom.location => string = "search";

  [@bs.send]
  external pushState:
    (Dom.history, [@bs.as {json|null|json}] _, [@bs.as ""] _, ~href: string) =>
    unit =
    "";

  [@bs.send]
  external replaceState:
    (Dom.history, [@bs.as {json|null|json}] _, [@bs.as ""] _, ~href: string) =>
    unit =
    "";

  [@bs.val] external event: 'a = "Event";

  [@bs.new] external makeEventIE11Compatible: string => Dom.event = "Event";

  [@bs.val] [@bs.scope "document"]
  external createEventNonIEBrowsers: string => Dom.event = "createEvent";

  [@bs.send]
  external initEventNonIEBrowsers: (Dom.event, string, bool, bool) => unit =
    "initEvent";

  let safeMakeEvent = eventName =>
    if (Js.typeof(event) == "function") {
      makeEventIE11Compatible(eventName);
    } else {
      let event = createEventNonIEBrowsers("Event");
      initEventNonIEBrowsers(event, eventName, true, true);
      event;
    };

  /* This is copied from array.ml. We want to cut dependencies for ReasonReact so
     that it's friendlier to use in size-constrained codebases */
  let arrayToList = a => {
    let rec tolist = (i, res) =>
      if (i < 0) {
        res;
      } else {
        tolist(i - 1, [Array.unsafe_get(a, i), ...res]);
      };
    tolist(Array.length(a) - 1, []);
  };
  /* if we ever roll our own parser in the future, make sure you test all url combinations
     e.g. foo.com/?#bar
     */
  /* sigh URLSearchParams doesn't work on IE11, edge16, etc. */
  /* actually you know what, not gonna provide search for now. It's a mess.
     We'll let users roll their own solution/data structure for now */
  let path = () =>
    switch ([%external window]) {
    | None => []
    | Some((window: Dom.window)) =>
      switch (window |> location |> pathname) {
      | ""
      | "/" => []
      | raw =>
        /* remove the preceeding /, which every pathname seems to have */
        let raw = Js.String.sliceToEnd(~from=1, raw);
        /* remove the trailing /, which some pathnames might have. Ugh */
        let raw =
          switch (Js.String.get(raw, Js.String.length(raw) - 1)) {
          | "/" => Js.String.slice(~from=0, ~to_=-1, raw)
          | _ => raw
          };
        raw |> Js.String.split("/") |> arrayToList;
      }
    };
  let hash = () =>
    switch ([%external window]) {
    | None => ""
    | Some((window: Dom.window)) =>
      switch (window |> location |> hash) {
      | ""
      | "#" => ""
      | raw =>
        /* remove the preceeding #, which every hash seems to have.
           Why is this even included in location.hash?? */
        raw |> Js.String.sliceToEnd(~from=1)
      }
    };
  let search = () =>
    switch ([%external window]) {
    | None => ""
    | Some((window: Dom.window)) =>
      switch (window |> location |> search) {
      | ""
      | "?" => ""
      | raw =>
        /* remove the preceeding ?, which every search seems to have. */
        raw |> Js.String.sliceToEnd(~from=1)
      }
    };
  let push = path =>
    switch ([%external history], [%external window]) {
    | (None, _)
    | (_, None) => ()
    | (Some((history: Dom.history)), Some((window: Dom.window))) =>
      pushState(history, ~href=path);
      dispatchEvent(window, safeMakeEvent("popstate"));
    };
  let replace = path =>
    switch ([%external history], [%external window]) {
    | (None, _)
    | (_, None) => ()
    | (Some((history: Dom.history)), Some((window: Dom.window))) =>
      replaceState(history, ~href=path);
      dispatchEvent(window, safeMakeEvent("popstate"));
    };
  type url = {
    path: list(string),
    hash: string,
    search: string,
  };
  type watcherID = unit => unit;
  let url = () => {path: path(), hash: hash(), search: search()};
  /* alias exposed publicly */
  let dangerouslyGetInitialUrl = url;
  let watchUrl = callback =>
    switch ([%external window]) {
    | None => (() => ())
    | Some((window: Dom.window)) =>
      let watcherID = () => callback(url());
      addEventListener(window, "popstate", watcherID);
      watcherID;
    };
  let unwatchUrl = watcherID =>
    switch ([%external window]) {
    | None => ()
    | Some((window: Dom.window)) =>
      removeEventListener(window, "popstate", watcherID)
    };
};

type sidEffectualState('state, 'retainedProps, 'action) = {
  sideEffects: array(self('state, 'retainedProps, 'action) => unit),
  state: 'state,
};
/*
 let make = (~foo, ~bar, _) => {
   ...component,
   render: (_) => <div/>
 };

 [@react.component]
 let make = (~foo, ~bar, _) => ReasonReact.useRecordApi({
   ...component,
   render: _ => <div />
 });
 */
let useRecordApi = componentSpec => {
  open React.Ref;

  let initialState = React.useMemo0(componentSpec.initialState);
  let unmountSideEffects = React.useRef([||]);
  let ({sideEffects, state}, send) =
    React.useReducer(
      (fullState, action) => {
        let recordApiStateReturn =
          componentSpec.reducer(action, fullState.state);
        switch (recordApiStateReturn) {
        | NoUpdate => fullState /* useReducer returns of the same value will not rerender the component */
        | Update(state) => {sideEffects: [||], state}
        | SideEffects(sideEffect) => {
            ...fullState,
            sideEffects:
              Js.Array.concat(fullState.sideEffects, [|sideEffect|]),
          }
        | UpdateWithSideEffects(state, sideEffect) => {
            sideEffects:
              Js.Array.concat(fullState.sideEffects, [|sideEffect|]),
            state,
          }
        };
      },
      {sideEffects: [||], state: initialState},
    );

  let rec self = {
    handle: (fn, payload) => fn(payload, self),
    retainedProps: componentSpec.retainedProps,
    send,
    state,
    onUnmount: sideEffect =>
      Js.Array.push(sideEffect, unmountSideEffects->current)->ignore,
  };

  let state = componentSpec.willReceiveProps(self);

  let rec self = {
    handle: (fn, payload) => fn(payload, self),
    retainedProps: componentSpec.retainedProps,
    send,
    state,
    onUnmount: sideEffect =>
      Js.Array.push(sideEffect, unmountSideEffects->current)->ignore,
  };

  let oldSelf = React.useRef(self);

  let hasBeenCalled = React.useRef(false);

  let _mountUnmountEffect =
    React.useLayoutEffect0(() => {
      componentSpec.didMount(self);
      Some(
        () => {
          Js.Array.forEach(fn => fn(), unmountSideEffects->current);
          /* shouldn't be needed but like - better safe than sorry? */
          unmountSideEffects->setCurrent([||]);
          componentSpec.willUnmount(self);
        },
      );
    });
  let _didUpdateEffect =
    React.useLayoutEffect(() => {
      if (!hasBeenCalled->current) {
        componentSpec.didUpdate({oldSelf: oldSelf->current, newSelf: self});
      } else {
        hasBeenCalled->setCurrent(true);
      };
      Js.Array.forEach(fn => fn(self), sideEffects);

      oldSelf->setCurrent(self);
      None;
    });

  let mostRecentAllowedRender =
    React.useRef(React.useMemo0(() => componentSpec.render(self)));

  if (hasBeenCalled->current
      && oldSelf->current.state !== self.state
      && componentSpec.shouldUpdate({
           oldSelf: oldSelf->current,
           newSelf: self,
         })) {
    componentSpec.willUpdate({oldSelf: oldSelf->current, newSelf: self});
    mostRecentAllowedRender->setCurrent(componentSpec.render(self));
  };
  mostRecentAllowedRender->current;
};