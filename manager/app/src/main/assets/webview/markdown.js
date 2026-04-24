window.onerror = (msg, url, line, column, error) => {
  window.github._sendMessage('error', {
    message: msg
  })
}

/**
 * @typedef {object} Rect
 * @property {number} X - The X coordinate
 * @property {number} Y - The Y coordinate
 * @property {number} Width - The width
 * @property {number} Height - The height
 */

/**
 * Object for configuring and controlling the page content
 */
class GitHub {
  /**
   * Initialize
   */
  constructor () {
  }

  /**
   * Send a message to the native app.
   *
   * @param {string} name Name of the message
   * @param {object} message Message content
   */
  _sendMessage (name, message) {
    const payload = message
    payload.messageName = name
    payload.id = this._id

    window.native.sendMessage(JSON.stringify(payload))
  }

  /**
   * Listen for events and send a height event when they fire
   */
  _addHeightListener () {
    if (typeof ResizeObserver !== 'undefined') {
      const resizeObserver = new ResizeObserver(entries => {
        this._updateHeight()
      })
      resizeObserver.observe(document.body)
    } else {
      this._addElementHeightListener('details', 'toggle')
      this._addElementHeightListener('video', 'resize')
      this._addElementHeightListener('img, svg, video', 'load')
      this._addElementHeightListener('img, svg, video', 'error')
      this._updateHeight()
    }
  }

  /**
   * If ResizeObserver is undefined, we have to observe the height of different elements.
   * This is **not** the preferred method as it does not account for all height changes by default.
   * Fixes: https://github.com/github/mobile-android/issues/3867
   *
   * @param {string} querySelector Query selector for elements
   * @param {string} eventName Event name to listen to
   */
  _addElementHeightListener (querySelector, eventName) {
    const elements = Array.from(document.querySelectorAll(querySelector))
    for (const element of elements) {
      element.addEventListener(eventName, () => {
        const height = document.body.scrollHeight
        this._updateHeight(height)
      })
    }
  }

  /**
   * Send a height to the app
   *
   * @param {int} height The new height to send to the app
   */
  _updateHeight () {
    const height = document.body.scrollHeight
    if (height === this._lastHeight) {
      return
    }

    this._lastHeight = height
    this._sendMessage('height', { height: height })
  }

  /**
   * Unescape `raw.githubusercontent.com` paths which contain escaped query parameters
   * See HACK #26
   */
  _unescapeImageURLs () {
    const images = Array.from(document.querySelectorAll('img'))
    for (const image of images) {
      if (!image.src) {
        continue
      }
      const imgSrc = new URL(image.src)
      if (imgSrc.host === 'raw.githubusercontent.com') {
        const pathWithQueryParams = decodeURIComponent(imgSrc.pathname)
        const newPath = new URL(pathWithQueryParams, 'https://raw.githubusercontent.com')

        // add back any query parameters from the original URL
        const searchParams = imgSrc.searchParams
        for (const [key, value] of searchParams) {
          newPath.searchParams.append(key, value)
        }

        image.src = newPath.href
      }
    }
  }

  /**
  * Workaround for https://github.com/github/mobile/issues/3093
  */
  _applyImageLoadingWorkaround () {
      document.querySelectorAll('source, img').forEach((node) => {
        ['srcset', 'src'].forEach((attrName) => {
          const attr = node.getAttribute(attrName)
          if (
            (attr !== null) &&
            (attr.startsWith('https://github.com')) &&
            (attr.indexOf('/blob/') !== -1)
          ) {
            node.setAttribute(attrName, attr.replace('/blob/', '/raw/'))
          }
        })
      })
    }

  /**
   * Post-process and setup listeners after changing the main content
   */
  _didLoad () {
    // Clear the height cache
    this._lastHeight = 0

    // Post-process content
    this._unescapeImageURLs()

    // Add a height listener
    this._addHeightListener()

    // Hack #19
    this._applyImageLoadingWorkaround()
  }

  /**
   * Load content into the web view
   *
   * @param {string} id
   * @param {string} html Content to load
   * @param {boolean} isCommitSuggestedChangesEnabled to indicate if we show the commit button
   * @param {boolean} isTaskListCompletionEnabled to indicate if we should enable task list checkboxes
   * @param {overridePos} checkbox position to override the check state on
   * @param {overrideVal} checked state for {overridePos}
   */
  load (id, html, isCommitSuggestedChangesEnabled, isTaskListCompletionEnabled, overridePos, overrideVal) {
    this._id = id
    const content = document.getElementById('content')

    if (content.innerHTML === html && content.getAttribute('overridePos') === overridePos) {
      this._updateHeight()
      return
    }

    content.setAttribute('overridePos', overridePos)

    // Set the content
    content.innerHTML = html

    // Add a commit suggested changes button
    if (isCommitSuggestedChangesEnabled) {
      for (const suggestedChangeElement of document.getElementsByClassName('js-suggested-changes-blob')) {
        // Add tabindex to table rows to avoid focus on the line number
        const tds = suggestedChangeElement.getElementsByTagName('td')
        for (const td of tds) {
          td.setAttribute('tabindex', '-1')
        }

        // Add tabindex to tables to make them focusable as a unique element
        const tables = suggestedChangeElement.getElementsByTagName('table')
        for (const table of tables) {
          table.setAttribute('tabindex', '0')
        }

        const buttonContainers = suggestedChangeElement.getElementsByClassName('js-apply-changes')
        if (buttonContainers && buttonContainers.length > 0) {
          this._addSuggestedChangesButton(
            buttonContainers[0],
            suggestedChangeElement.id,
            suggestedChangeElement.outerHTML
          )
        }
      }
    }

    const checkBoxes = document.querySelectorAll(".task-list-item-checkbox, .tlb-checkbox, .tlb-issue-reference-number")
    for (let i = 0; i < checkBoxes.length; i++) {
        if(i == overridePos){
            checkBoxes[i].checked = overrideVal === 'true'
        }
        if(isTaskListCompletionEnabled){
            checkBoxes[i].disabled = false
            checkBoxes[i].addEventListener('change', (event) => {
                this._commitTaskItemChanged(i, event.target.checked)
                });
        }
        else{
            checkBoxes[i].disabled = true
        }
    }

    this._didLoad()
    this._updateHeight()
  }

  /**
   * Load a suggested changes button into the given element
   *
   * @param {element} container to add the suggested changes button
   * @param {id} the suggested change id
   * @param {previewHTML} the full suggested change HTML
   */
  _addSuggestedChangesButton (container, id, previewHTML) {
    container.innerHTML =
        "<div style=\"display: flex; justify-content: center;\">" +
        "<button onclick=\"window.github._commitSuggestedChange('" + id + "', '" + escape(previewHTML) + "')\" " +
        "style=\"" +
        "padding: 10px 0px; " +
        "margin: 10px; " +
        "display: flex; " +
        "align-items: center; " +
        "justify-content: center; " +
        "width: 100%; " +
        "text-transform: uppercase; " +
        "font-weight: 500; " +
        "font-size: 14px; " +
        "letter-spacing: 0.05em; " +
        "background-color: var(--backgroundElevatedSecondary); " +
        "color: var(--link); " +
        "border-color: var(--border); " +
        "border-radius: 6px; " +
        "border-width: 1px; " +
        "outline: none; " +
        "\" onfocus=\"this.style.backgroundColor='var(--suggestedChangeCommitButtonBackgroundDisabled)';\" " +
        "onblur=\"this.style.backgroundColor='var(--backgroundElevatedSecondary)';\">" +
        "<svg style=\"fill: var(--link); margin: 4px; 0px; 0px; 0px;\" " +
        "viewBox=\"0 0 16 16\" width=\"16\" height=\"16\"><path fill-rule=\"evenodd\" " +
        "d=\"M10.5 7.75a2.5 2.5 0 11-5 0 2.5 2.5 0 015 0zm1.43.75a4.002 4.002 0 01-7.86 0H.75a.75.75 0 " +
        "110-1.5h3.32a4.001 4.001 0 017.86 0h3.32a.75.75 0 110 1.5h-3.32z\"></path></svg>" +
        "Commit" +
        "</button>" +
        "</div>";
  }

  /**
   * Commit the suggested change back in the native view
   *
   * @param {id} the suggested change id
   * @param {escapedPreviewHTML} the full, escaped suggested change HTML
   */
  _commitSuggestedChange (id, escapedPreviewHTML) {
    this._sendMessage('commit_suggestion', {
      suggestionId: id,
      previewHTML: unescape(escapedPreviewHTML)
    })
  }

    /**
     * change the task list checkbox value
     *
     * @param {position} the position of the checkbox in the task list
     * @param {checked} checked value
     */
    _commitTaskItemChanged (position, checked) {
      this._sendMessage('task_changed', {
        taskPosition: position,
        taskChecked: checked
      })
    }

  /**
   * Load content into the web view
   *
   * @param {anchor} anchor
   */
  getAnchorPosition (anchor) {
    var position = this._positionOf(anchor)
    this._sendMessage('scroll_to', {
        posY: position,
        anchor: anchor
    })
  }

  /**
   * Get the position of an element by ID
   *
   * @param {string} elementID Name of the anchor tapped
   * @returns {number} The element's top Y position
   */
  _positionOf (elementID) {
    let element = this._getElementById(elementID)
    if (element === null) {
      element = this._getElementByName(elementID)
    }
    if (element === null) {
      return null
    }
    let rect = element.getBoundingClientRect()

    // Workaround for https://github.com/github/mobile-ios/issues/10546
    // The problem is that for some anchors we get a 0-rect which makes the app scroll to the
    // top of the screen instead of the location of the anchor. Here we just give a second try
    // and fetch the bounding client rectangle of the parent node of the anchor.
    if (rect.y === 0 && rect.x === 0 && rect.width === 0 && rect.height === 0) {
      const parent = element.parentElement
      if (parent === null) {
        return null
      }
      rect = parent.getBoundingClientRect()
    }

    return rect.y
  }


  /**
   * Get the DOM element by the given id in a case-insensitive way. We first try
   * to find an element which as the exact given id. If we cannot find any, we
   * search again with lowercased id.
   *
   * @param {string} id of the element that we want to find
   * @returns {HTMLElement} element or `null` if not found.
   */
  _getElementById (id) {
    const decodedID = decodeURIComponent(id)
    const possibleElements = [
      decodedID,
      decodedID.toLowerCase(),
      `user-content-${decodedID}`,
      `user-content-${decodedID.toLowerCase()}`
    ]

    return possibleElements
      .map((element) => document.getElementById(element))
      .find((element) => element !== null)
  }

  /**
   * Get the DOM element by the given name in a case-insensitive way. We first try
   * to find an element which as the exact given name. If we cannot find any, we
   * search again with lowercased name.
   *
   * @param {string} name of the element that we want to find
   * @returns {HTMLElement} element or `null` if not found.
   */
  _getElementByName (name) {
    let elements = document.getElementsByName(name)
    if (elements.length === 0) {
      elements = document.getElementsByName(name.toLowerCase())
    }
    if (elements.length === 0) {
      return null
    }
    return elements[0]
  }
}

const start = () => {
  window.github = new GitHub()
}
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', start)
} else {
  start()
}

if (typeof exports !== 'undefined') {
  exports.GitHub = GitHub
}
