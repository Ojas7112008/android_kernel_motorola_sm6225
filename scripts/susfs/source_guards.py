#!/usr/bin/env python3
"""Remove selected feature guards for byte comparisons with native source.

Preserves unrelated directives. Handles nested blocks and the original #else
branch; unsupported conditions stay visible so restoration checks fail closed.
"""
import re


def without_feature_guards(text, features):
    selected = set(features)
    lines = text.splitlines(True)

    def condition(directive):
        match = re.match(r'\s*#\s*(ifdef|ifndef|if|elif)\s+(.+)', directive)
        if not match:
            return None
        kind, expr = match.groups()
        expr = re.sub(r'/\*.*?\*/|//.*', '', expr).strip()
        if kind in ('ifdef', 'ifndef'):
            if expr not in selected:
                return None
            return kind == 'ifndef'
        names = set(re.findall(r'CONFIG_\w+', expr))
        if not names or not names <= selected:
            return None
        expr = re.sub(r'defined\s*\(\s*CONFIG_\w+\s*\)|defined\s+CONFIG_\w+|CONFIG_\w+', '0', expr)
        if re.search(r'[^01()!|&\s]', expr):
            return None
        expr = expr.replace('&&', ' and ').replace('||', ' or ').replace('!', ' not ')
        return bool(eval(expr, {'__builtins__': {}}, {}))

    def walk(start, end):
        result = []
        i = start
        while i < end:
            if not re.match(r'\s*#\s*(if|ifdef|ifndef)\b', lines[i]):
                result.append(lines[i])
                i += 1
                continue
            depth, j, branches = 1, i + 1, [(lines[i], i + 1)]
            while j < end and depth:
                if re.match(r'\s*#\s*(if|ifdef|ifndef)\b', lines[j]):
                    depth += 1
                elif re.match(r'\s*#\s*endif\b', lines[j]):
                    depth -= 1
                elif depth == 1 and re.match(r'\s*#\s*(else|elif)\b', lines[j]):
                    branches.append((lines[j], j + 1))
                if depth:
                    j += 1
            if depth:
                raise ValueError('Unbalanced source guard at line ' + str(i + 1))
            known = condition(lines[i])
            if any(not re.match(r'\s*#\s*else\b', directive) and condition(directive) is None
                   for directive, _ in branches):
                # A mixed #elif chain cannot be evaluated from selected flags.
                # Preserve it so native restoration fails visibly, not silently.
                known = None
            for index, (directive, body_start) in enumerate(branches):
                body_end = branches[index + 1][1] - 1 if index + 1 < len(branches) else j
                if known is None:
                    result.append(directive)
                    result.append(walk(body_start, body_end))
                elif re.match(r'\s*#\s*else\b', directive) or condition(directive):
                    result.append(walk(body_start, body_end))
                    break
            if known is None:
                result.append(lines[j])
            i = j + 1
        return ''.join(result)

    return walk(0, len(lines))
